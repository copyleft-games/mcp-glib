/*
 * test-mux-transport.c - Unit tests for McpMuxTransport
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define MCP_COMPILATION
#include "mcp-mux-transport.h"
#include "mcp-transport.h"
#include "mcp-error.h"
#undef MCP_COMPILATION

#include <json-glib/json-glib.h>

/* ── shared test plumbing ──────────────────────────────────────────── */

typedef struct
{
    GMainContext *context;             /* borrowed */
    GQueue       *captured_sends;      /* JsonNode* (owned)  */
    GQueue       *captured_receives;   /* JsonNode* (owned)  */
    GQueue       *state_changes;       /* (old<<8)|new       */
    guint         destroy_count;
    GError       *captured_error;      /* owned              */
} TestCtx;

static TestCtx *
test_ctx_new (GMainContext *ctx)
{
    TestCtx *t = g_new0 (TestCtx, 1);

    t->context           = ctx;
    t->captured_sends    = g_queue_new ();
    t->captured_receives = g_queue_new ();
    t->state_changes     = g_queue_new ();
    return t;
}

static void
test_ctx_free (TestCtx *ctx)
{
    g_queue_free_full (ctx->captured_sends,    (GDestroyNotify) json_node_unref);
    g_queue_free_full (ctx->captured_receives, (GDestroyNotify) json_node_unref);
    g_queue_free (ctx->state_changes);
    g_clear_error (&ctx->captured_error);
    g_free (ctx);
}

static void
send_cb (McpMuxTransport *self,
         JsonNode        *frame,
         gpointer         user_data)
{
    TestCtx *ctx = user_data;

    (void) self;
    g_queue_push_tail (ctx->captured_sends, json_node_ref (frame));
}

static void
send_cb_destroy (gpointer user_data)
{
    TestCtx *ctx = user_data;
    ctx->destroy_count++;
}

static void
on_message_received (McpTransport *transport,
                     JsonNode     *frame,
                     gpointer      user_data)
{
    TestCtx *ctx = user_data;

    (void) transport;
    g_queue_push_tail (ctx->captured_receives, json_node_ref (frame));
}

static void
on_state_changed (McpTransport      *transport,
                  McpTransportState  old_state,
                  McpTransportState  new_state,
                  gpointer           user_data)
{
    TestCtx *ctx = user_data;

    (void) transport;
    g_queue_push_tail (ctx->state_changes,
                       GUINT_TO_POINTER (((guint) old_state << 8)
                                         | (guint) new_state));
}

static void
on_error (McpTransport *transport,
          GError       *error,
          gpointer      user_data)
{
    TestCtx *ctx = user_data;

    (void) transport;
    g_clear_error (&ctx->captured_error);
    ctx->captured_error = g_error_copy (error);
}

/* Pump @ctx until @cond returns TRUE or a 2 s deadline elapses. */
static gboolean
pump_until (GMainContext *ctx, gboolean (*cond) (gpointer), gpointer data)
{
    gint64 deadline = g_get_monotonic_time () + 2 * G_USEC_PER_SEC;

    while (!cond (data) && g_get_monotonic_time () < deadline)
    {
        g_main_context_iteration (ctx, FALSE);
        if (cond (data))
            return TRUE;
        g_usleep (1000);
    }
    return cond (data);
}

static gboolean
has_one_receive (gpointer user_data)
{
    TestCtx *ctx = user_data;
    return g_queue_get_length (ctx->captured_receives) >= 1;
}

static gboolean
has_n_receives (TestCtx *ctx, guint n)
{
    return g_queue_get_length (ctx->captured_receives) >= n;
}

static gboolean
has_three_receives (gpointer user_data)
{
    return has_n_receives (user_data, 3);
}

static JsonNode *
make_object_frame (const gchar *key, const gchar *value)
{
    g_autoptr(JsonBuilder) b = json_builder_new ();

    json_builder_begin_object (b);
    json_builder_set_member_name (b, key);
    json_builder_add_string_value (b, value);
    json_builder_end_object (b);
    return json_builder_get_root (b);
}

/* Block-style synchronous async runner: drives the loop until @result
 * is non-NULL. */
typedef struct { GAsyncResult *result; GMainContext *ctx; } SyncSlot;

static void
sync_capture (GObject *src, GAsyncResult *res, gpointer data)
{
    SyncSlot *slot = data;
    (void) src;
    slot->result = g_object_ref (res);
}

static GAsyncResult *
run_sync (GMainContext *ctx,
          void (*starter) (gpointer, GAsyncReadyCallback, gpointer),
          gpointer arg)
{
    SyncSlot slot = { NULL, ctx };
    gint64 deadline;

    starter (arg, sync_capture, &slot);

    deadline = g_get_monotonic_time () + 2 * G_USEC_PER_SEC;
    while (slot.result == NULL && g_get_monotonic_time () < deadline)
        g_main_context_iteration (ctx, FALSE);
    return slot.result;
}

/* ── tests: basic construction ─────────────────────────────────────── */

static void
test_construct_default_context (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);

    g_assert_true (MCP_IS_TRANSPORT (t));
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (t)),
                     ==, MCP_TRANSPORT_STATE_DISCONNECTED);
    g_assert_nonnull (mcp_mux_transport_get_context (t));
    /* When NULL is passed, we should land on the thread-default. */
    g_assert_true (mcp_mux_transport_get_context (t)
                   == g_main_context_get_thread_default ()
                   || mcp_mux_transport_get_context (t)
                      == g_main_context_default ());
}

static void
test_construct_explicit_context (void)
{
    g_autoptr(GMainContext) mctx = g_main_context_new ();
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (mctx);

    g_assert_true (mcp_mux_transport_get_context (t) == mctx);
    g_assert_false (mcp_transport_is_connected (MCP_TRANSPORT (t)));
}

/* ── tests: send path ──────────────────────────────────────────────── */

static void
test_send_routes_to_callback (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);
    TestCtx *ctx = test_ctx_new (NULL);
    g_autoptr(JsonNode) frame = make_object_frame ("k", "v");

    mcp_mux_transport_set_send_callback (t, send_cb, ctx, send_cb_destroy);
    mcp_mux_transport_set_connected (t, TRUE);

    mcp_transport_send_message_async (MCP_TRANSPORT (t), frame,
                                       NULL, NULL, NULL);

    g_assert_cmpuint (g_queue_get_length (ctx->captured_sends), ==, 1);

    /* Re-setting the callback to NULL must run the destroy notify. */
    mcp_mux_transport_set_send_callback (t, NULL, NULL, NULL);
    g_assert_cmpuint (ctx->destroy_count, ==, 1);

    test_ctx_free (ctx);
}

static void
test_send_destroy_called_on_finalize (void)
{
    McpMuxTransport *t = mcp_mux_transport_new (NULL);
    TestCtx *ctx = test_ctx_new (NULL);

    mcp_mux_transport_set_send_callback (t, send_cb, ctx, send_cb_destroy);
    g_object_unref (t);

    /* Finalize must run the destroy notify even when the callback
     * isn't explicitly cleared first. */
    g_assert_cmpuint (ctx->destroy_count, ==, 1);

    test_ctx_free (ctx);
}

static void
test_send_destroy_runs_only_on_replacement (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);
    TestCtx *ctx = test_ctx_new (NULL);
    guint other_count = 0;

    mcp_mux_transport_set_send_callback (t, send_cb, ctx, send_cb_destroy);
    g_assert_cmpuint (ctx->destroy_count, ==, 0);

    /* Replace the callback — destroy notify for the OLD callback's
     * user_data should fire exactly once. */
    mcp_mux_transport_set_send_callback (t, send_cb, &other_count, NULL);
    g_assert_cmpuint (ctx->destroy_count, ==, 1);

    /* Re-replacing without a destroy notify should NOT touch the
     * other_count (callback's data has no destroyer). */
    mcp_mux_transport_set_send_callback (t, NULL, NULL, NULL);
    g_assert_cmpuint (other_count, ==, 0);

    test_ctx_free (ctx);
}

static void
sender (gpointer arg, GAsyncReadyCallback cb, gpointer data)
{
    struct { McpTransport *t; JsonNode *f; } *p = arg;
    mcp_transport_send_message_async (p->t, p->f, NULL, cb, data);
}

static void
test_send_fails_when_disconnected (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);
    TestCtx *ctx = test_ctx_new (NULL);
    g_autoptr(JsonNode) frame = make_object_frame ("dropped", "1");
    g_autoptr(GAsyncResult) result = NULL;
    g_autoptr(GError) error = NULL;
    struct { McpTransport *t; JsonNode *f; } args =
        { MCP_TRANSPORT (t), frame };

    /* Callback registered but never set CONNECTED. */
    mcp_mux_transport_set_send_callback (t, send_cb, ctx, NULL);

    result = run_sync (g_main_context_default (), sender, &args);
    g_assert_nonnull (result);
    g_assert_false (mcp_transport_send_message_finish (MCP_TRANSPORT (t),
                                                        result, &error));
    g_assert_error (error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_cmpuint (g_queue_get_length (ctx->captured_sends), ==, 0);

    test_ctx_free (ctx);
}

static void
test_send_fails_when_no_callback (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);
    g_autoptr(JsonNode) frame = make_object_frame ("no-cb", "1");
    g_autoptr(GAsyncResult) result = NULL;
    g_autoptr(GError) error = NULL;
    struct { McpTransport *t; JsonNode *f; } args =
        { MCP_TRANSPORT (t), frame };

    /* CONNECTED but no callback. */
    mcp_mux_transport_set_connected (t, TRUE);

    result = run_sync (g_main_context_default (), sender, &args);
    g_assert_nonnull (result);
    g_assert_false (mcp_transport_send_message_finish (MCP_TRANSPORT (t),
                                                        result, &error));
    g_assert_error (error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
}

/* ── tests: dispatch / inbound ────────────────────────────────────── */

static void
test_dispatch_emits_message_received (void)
{
    g_autoptr(GMainContext) mctx = g_main_context_new ();
    g_autoptr(McpMuxTransport) t = NULL;
    TestCtx *ctx = test_ctx_new (mctx);
    g_autoptr(JsonNode) frame = make_object_frame ("hello", "world");
    gulong sig;

    g_main_context_push_thread_default (mctx);

    t = mcp_mux_transport_new (mctx);
    sig = g_signal_connect (t, "message-received",
                            G_CALLBACK (on_message_received), ctx);

    mcp_mux_transport_set_connected (t, TRUE);
    mcp_mux_transport_dispatch_frame (t, frame);

    g_assert_true (pump_until (mctx, has_one_receive, ctx));
    g_assert_cmpuint (g_queue_get_length (ctx->captured_receives), ==, 1);

    g_signal_handler_disconnect (t, sig);
    g_main_context_pop_thread_default (mctx);

    test_ctx_free (ctx);
}

static void
test_dispatch_dropped_when_disconnected (void)
{
    g_autoptr(GMainContext) mctx = g_main_context_new ();
    g_autoptr(McpMuxTransport) t = NULL;
    TestCtx *ctx = test_ctx_new (mctx);
    g_autoptr(JsonNode) frame = make_object_frame ("ignored", "1");
    gulong sig;

    g_main_context_push_thread_default (mctx);

    t = mcp_mux_transport_new (mctx);
    sig = g_signal_connect (t, "message-received",
                            G_CALLBACK (on_message_received), ctx);

    /* DISCONNECTED — the frame is reffed and queued by dispatch but
     * the signal handler is suppressed on the dispatched run because
     * state is not CONNECTED. */
    mcp_mux_transport_dispatch_frame (t, frame);
    while (g_main_context_iteration (mctx, FALSE))
        ;
    g_assert_cmpuint (g_queue_get_length (ctx->captured_receives), ==, 0);

    g_signal_handler_disconnect (t, sig);
    g_main_context_pop_thread_default (mctx);
    test_ctx_free (ctx);
}

static void
test_dispatch_multiple_in_order (void)
{
    g_autoptr(GMainContext) mctx = g_main_context_new ();
    g_autoptr(McpMuxTransport) t = NULL;
    TestCtx *ctx = test_ctx_new (mctx);
    g_autoptr(JsonNode) a = make_object_frame ("seq", "a");
    g_autoptr(JsonNode) b = make_object_frame ("seq", "b");
    g_autoptr(JsonNode) c = make_object_frame ("seq", "c");
    gulong sig;
    JsonNode *got;

    g_main_context_push_thread_default (mctx);
    t = mcp_mux_transport_new (mctx);
    sig = g_signal_connect (t, "message-received",
                            G_CALLBACK (on_message_received), ctx);
    mcp_mux_transport_set_connected (t, TRUE);

    mcp_mux_transport_dispatch_frame (t, a);
    mcp_mux_transport_dispatch_frame (t, b);
    mcp_mux_transport_dispatch_frame (t, c);

    g_assert_true (pump_until (mctx, has_three_receives, ctx));

    /* g_main_context_invoke_full queues at G_PRIORITY_DEFAULT; FIFO
     * order is preserved across dispatch calls. */
    got = g_queue_pop_head (ctx->captured_receives);
    g_assert_cmpstr (json_object_get_string_member (json_node_get_object (got),
                                                     "seq"), ==, "a");
    json_node_unref (got);
    got = g_queue_pop_head (ctx->captured_receives);
    g_assert_cmpstr (json_object_get_string_member (json_node_get_object (got),
                                                     "seq"), ==, "b");
    json_node_unref (got);
    got = g_queue_pop_head (ctx->captured_receives);
    g_assert_cmpstr (json_object_get_string_member (json_node_get_object (got),
                                                     "seq"), ==, "c");
    json_node_unref (got);

    g_signal_handler_disconnect (t, sig);
    g_main_context_pop_thread_default (mctx);
    test_ctx_free (ctx);
}

typedef struct
{
    McpMuxTransport *transport;
    JsonNode        *frame;
} ThreadDispatchArgs;

static gpointer
thread_dispatch_fn (gpointer data)
{
    ThreadDispatchArgs *args = data;
    mcp_mux_transport_dispatch_frame (args->transport, args->frame);
    return NULL;
}

static void
test_dispatch_from_worker_thread (void)
{
    g_autoptr(GMainContext) mctx = g_main_context_new ();
    g_autoptr(McpMuxTransport) t = NULL;
    TestCtx *ctx = test_ctx_new (mctx);
    g_autoptr(JsonNode) frame = make_object_frame ("from", "thread");
    GThread *th;
    ThreadDispatchArgs args;
    gulong sig;

    g_main_context_push_thread_default (mctx);
    t = mcp_mux_transport_new (mctx);
    sig = g_signal_connect (t, "message-received",
                            G_CALLBACK (on_message_received), ctx);
    mcp_mux_transport_set_connected (t, TRUE);

    args.transport = t;
    args.frame     = frame;
    th = g_thread_new ("disp", thread_dispatch_fn, &args);

    g_assert_true (pump_until (mctx, has_one_receive, ctx));

    g_thread_join (th);
    g_signal_handler_disconnect (t, sig);
    g_main_context_pop_thread_default (mctx);
    test_ctx_free (ctx);
}

/* ── tests: state machine ──────────────────────────────────────────── */

static void
test_state_changes_emitted (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);
    TestCtx *ctx = test_ctx_new (NULL);
    gulong sig;
    guint encoded;

    sig = g_signal_connect (t, "state-changed",
                            G_CALLBACK (on_state_changed), ctx);

    mcp_mux_transport_set_connected (t, TRUE);
    mcp_mux_transport_set_connected (t, TRUE);    /* idempotent */
    mcp_mux_transport_set_connected (t, FALSE);
    mcp_mux_transport_set_connected (t, FALSE);   /* idempotent */

    g_assert_cmpuint (g_queue_get_length (ctx->state_changes), ==, 2);

    encoded = GPOINTER_TO_UINT (g_queue_pop_head (ctx->state_changes));
    g_assert_cmpuint (encoded, ==, ((guint) MCP_TRANSPORT_STATE_DISCONNECTED << 8)
                                    | (guint) MCP_TRANSPORT_STATE_CONNECTED);
    encoded = GPOINTER_TO_UINT (g_queue_pop_head (ctx->state_changes));
    g_assert_cmpuint (encoded, ==, ((guint) MCP_TRANSPORT_STATE_CONNECTED << 8)
                                    | (guint) MCP_TRANSPORT_STATE_DISCONNECTED);

    g_signal_handler_disconnect (t, sig);
    test_ctx_free (ctx);
}

static void
connect_starter (gpointer arg, GAsyncReadyCallback cb, gpointer data)
{
    mcp_transport_connect_async (MCP_TRANSPORT (arg), NULL, cb, data);
}

static void
test_connect_advances_state (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);
    g_autoptr(GAsyncResult) result = NULL;
    g_autoptr(GError) error = NULL;

    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (t)), ==,
                      MCP_TRANSPORT_STATE_DISCONNECTED);

    result = run_sync (g_main_context_default (), connect_starter, t);
    g_assert_nonnull (result);
    g_assert_true (mcp_transport_connect_finish (MCP_TRANSPORT (t),
                                                  result, &error));
    g_assert_no_error (error);
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (t)), ==,
                      MCP_TRANSPORT_STATE_CONNECTED);
    g_assert_true (mcp_transport_is_connected (MCP_TRANSPORT (t)));
}

static void
test_connect_when_already_connected (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);
    g_autoptr(GAsyncResult) result = NULL;
    g_autoptr(GError) error = NULL;
    TestCtx *ctx = test_ctx_new (NULL);
    gulong sig;

    mcp_mux_transport_set_connected (t, TRUE);
    sig = g_signal_connect (t, "state-changed",
                            G_CALLBACK (on_state_changed), ctx);

    /* connect_async when already CONNECTED should succeed without
     * firing a state-change. */
    result = run_sync (g_main_context_default (), connect_starter, t);
    g_assert_true (mcp_transport_connect_finish (MCP_TRANSPORT (t),
                                                  result, &error));
    g_assert_no_error (error);
    g_assert_cmpuint (g_queue_get_length (ctx->state_changes), ==, 0);

    g_signal_handler_disconnect (t, sig);
    test_ctx_free (ctx);
}

static void
disconnect_starter (gpointer arg, GAsyncReadyCallback cb, gpointer data)
{
    mcp_transport_disconnect_async (MCP_TRANSPORT (arg), NULL, cb, data);
}

static void
test_disconnect_advances_state (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);
    g_autoptr(GAsyncResult) result = NULL;
    g_autoptr(GError) error = NULL;

    mcp_mux_transport_set_connected (t, TRUE);
    g_assert_true (mcp_transport_is_connected (MCP_TRANSPORT (t)));

    result = run_sync (g_main_context_default (), disconnect_starter, t);
    g_assert_nonnull (result);
    g_assert_true (mcp_transport_disconnect_finish (MCP_TRANSPORT (t),
                                                     result, &error));
    g_assert_no_error (error);
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (t)), ==,
                      MCP_TRANSPORT_STATE_DISCONNECTED);
}

/* ── tests: error path ────────────────────────────────────────────── */

static void
test_error_from_host (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);
    TestCtx *ctx = test_ctx_new (NULL);
    g_autoptr(GError) err = g_error_new (MCP_ERROR,
                                          MCP_ERROR_TRANSPORT_ERROR,
                                          "carrier collapsed");
    gulong sig;

    sig = g_signal_connect (t, "error", G_CALLBACK (on_error), ctx);
    mcp_mux_transport_emit_error_from_host (t, err);

    g_assert_nonnull (ctx->captured_error);
    g_assert_error (ctx->captured_error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_cmpstr (ctx->captured_error->message, ==, "carrier collapsed");

    g_signal_handler_disconnect (t, sig);
    test_ctx_free (ctx);
}

/* ── tests: argument-validation guards ────────────────────────────── */

/* Each guard test installs g_test_expect_message to absorb the
 * critical that g_return_if_fail produces.  This validates the
 * defensive checks fire without aborting the test runner. */

static void
test_guard_dispatch_null_self (void)
{
    g_autoptr(JsonNode) frame = make_object_frame ("k", "v");

    g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*MCP_IS_MUX_TRANSPORT*");
    mcp_mux_transport_dispatch_frame (NULL, frame);
    g_test_assert_expected_messages ();
}

static void
test_guard_dispatch_null_frame (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);

    g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*frame != NULL*");
    mcp_mux_transport_dispatch_frame (t, NULL);
    g_test_assert_expected_messages ();
}

static void
test_guard_set_send_callback_null_self (void)
{
    g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*MCP_IS_MUX_TRANSPORT*");
    mcp_mux_transport_set_send_callback (NULL, NULL, NULL, NULL);
    g_test_assert_expected_messages ();
}

static void
test_guard_set_connected_null_self (void)
{
    g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*MCP_IS_MUX_TRANSPORT*");
    mcp_mux_transport_set_connected (NULL, TRUE);
    g_test_assert_expected_messages ();
}

static void
test_guard_emit_error_null_error (void)
{
    g_autoptr(McpMuxTransport) t = mcp_mux_transport_new (NULL);

    g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*error != NULL*");
    mcp_mux_transport_emit_error_from_host (t, NULL);
    g_test_assert_expected_messages ();
}

static void
test_guard_get_context_null (void)
{
    g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*MCP_IS_MUX_TRANSPORT*");
    g_assert_null (mcp_mux_transport_get_context (NULL));
    g_test_assert_expected_messages ();
}

/* ── tests: end-to-end round trip between two muxes ───────────────── */

typedef struct
{
    McpMuxTransport *peer;          /* borrowed; dispatch into this */
} BackToBackCtx;

static void
back_to_back_send (McpMuxTransport *self, JsonNode *frame, gpointer user_data)
{
    BackToBackCtx *b = user_data;
    (void) self;
    /* Simulate a wire: each transport's send is the other's
     * dispatch.  Serialise + reparse so we exercise the JSON path
     * any real carrier would take. */
    {
        g_autofree gchar *str = NULL;
        g_autoptr(JsonParser) p = json_parser_new ();
        JsonGenerator *g = json_generator_new ();

        json_generator_set_root (g, frame);
        str = json_generator_to_data (g, NULL);
        g_object_unref (g);

        if (json_parser_load_from_data (p, str, -1, NULL))
            mcp_mux_transport_dispatch_frame (b->peer,
                                              json_parser_get_root (p));
    }
}

static void
test_two_muxes_back_to_back (void)
{
    g_autoptr(GMainContext) mctx = g_main_context_new ();
    g_autoptr(McpMuxTransport) a = NULL;
    g_autoptr(McpMuxTransport) b = NULL;
    TestCtx *ctx_a = test_ctx_new (mctx);
    TestCtx *ctx_b = test_ctx_new (mctx);
    BackToBackCtx wire_a, wire_b;
    g_autoptr(JsonNode) frame_a = make_object_frame ("from", "a");
    g_autoptr(JsonNode) frame_b = make_object_frame ("from", "b");
    gulong sig_a, sig_b;

    g_main_context_push_thread_default (mctx);
    a = mcp_mux_transport_new (mctx);
    b = mcp_mux_transport_new (mctx);

    wire_a.peer = b;
    wire_b.peer = a;
    mcp_mux_transport_set_send_callback (a, back_to_back_send, &wire_a, NULL);
    mcp_mux_transport_set_send_callback (b, back_to_back_send, &wire_b, NULL);
    mcp_mux_transport_set_connected (a, TRUE);
    mcp_mux_transport_set_connected (b, TRUE);

    sig_a = g_signal_connect (a, "message-received",
                              G_CALLBACK (on_message_received), ctx_a);
    sig_b = g_signal_connect (b, "message-received",
                              G_CALLBACK (on_message_received), ctx_b);

    mcp_transport_send_message_async (MCP_TRANSPORT (a), frame_a,
                                       NULL, NULL, NULL);
    mcp_transport_send_message_async (MCP_TRANSPORT (b), frame_b,
                                       NULL, NULL, NULL);

    g_assert_true (pump_until (mctx, has_one_receive, ctx_a));
    g_assert_true (pump_until (mctx, has_one_receive, ctx_b));

    {
        JsonNode *r = g_queue_pop_head (ctx_a->captured_receives);
        g_assert_cmpstr (json_object_get_string_member (json_node_get_object (r),
                                                         "from"), ==, "b");
        json_node_unref (r);
        r = g_queue_pop_head (ctx_b->captured_receives);
        g_assert_cmpstr (json_object_get_string_member (json_node_get_object (r),
                                                         "from"), ==, "a");
        json_node_unref (r);
    }

    g_signal_handler_disconnect (a, sig_a);
    g_signal_handler_disconnect (b, sig_b);
    g_main_context_pop_thread_default (mctx);
    test_ctx_free (ctx_a);
    test_ctx_free (ctx_b);
}

/* ── tests: McpClient/McpServer over two muxes (integration) ─────── */

#include "mcp-client.h"
#include "mcp-server.h"
#include "mcp-session.h"
#include "mcp-tool.h"
#include "mcp-tool-provider.h"

static McpToolResult *
echo_tool_handler (McpServer    *server,
                   const gchar  *name,
                   JsonObject   *arguments,
                   gpointer      user_data)
{
    const gchar *text = "";
    McpToolResult *result;

    (void) server;
    (void) name;
    (void) user_data;

    if (arguments != NULL && json_object_has_member (arguments, "text"))
        text = json_object_get_string_member (arguments, "text");
    result = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_text (result, text);
    return result;
}

typedef struct
{
    GMainLoop *loop;
    McpToolResult *result;
    GError *error;
} CallCtx;

static void
on_call_done (GObject *source, GAsyncResult *res, gpointer data)
{
    CallCtx *cc = data;
    cc->result = mcp_client_call_tool_finish (MCP_CLIENT (source),
                                               res, &cc->error);
    g_main_loop_quit (cc->loop);
}

static void
on_client_connected (GObject *source, GAsyncResult *res, gpointer data)
{
    GError *err = NULL;
    (void) data;
    if (!mcp_client_connect_finish (MCP_CLIENT (source), res, &err))
    {
        g_clear_error (&err);
    }
}

static void
test_full_mcp_stack_over_muxes (void)
{
    g_autoptr(GMainContext) mctx = g_main_context_new ();
    g_autoptr(McpMuxTransport) client_t = NULL;
    g_autoptr(McpMuxTransport) server_t = NULL;
    g_autoptr(McpClient) client = NULL;
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpTool) echo  = NULL;
    BackToBackCtx wire_c, wire_s;
    CallCtx cc = { NULL, NULL, NULL };
    g_autoptr(JsonBuilder) ab = json_builder_new ();
    g_autoptr(JsonObject) args = NULL;
    JsonNode *root;
    g_autoptr(GMainLoop) loop = NULL;

    g_main_context_push_thread_default (mctx);

    client_t = mcp_mux_transport_new (mctx);
    server_t = mcp_mux_transport_new (mctx);

    /* Cross-wire the two transports. */
    wire_c.peer = server_t;
    wire_s.peer = client_t;
    mcp_mux_transport_set_send_callback (client_t, back_to_back_send,
                                          &wire_c, NULL);
    mcp_mux_transport_set_send_callback (server_t, back_to_back_send,
                                          &wire_s, NULL);
    mcp_mux_transport_set_connected (client_t, TRUE);
    mcp_mux_transport_set_connected (server_t, TRUE);

    server = mcp_server_new ("test-server", "1.0");
    echo   = mcp_tool_new ("echo", "echo the text arg back");
    mcp_server_add_tool (server, echo, echo_tool_handler, NULL, NULL);
    mcp_server_set_transport (server, MCP_TRANSPORT (server_t));
    mcp_server_start_async (server, NULL, NULL, NULL);

    client = mcp_client_new ("test-client", "1.0");
    mcp_client_set_transport (client, MCP_TRANSPORT (client_t));
    mcp_client_connect_async (client, NULL, on_client_connected, NULL);

    /* Pump until the client transitions to READY (initialization
     * handshake completes when both ends have exchanged initialize +
     * initialized).  Use a generous deadline. */
    {
        gint64 deadline = g_get_monotonic_time () + 3 * G_USEC_PER_SEC;
        while (mcp_session_get_state (MCP_SESSION (client))
               != MCP_SESSION_STATE_READY
               && g_get_monotonic_time () < deadline)
            g_main_context_iteration (mctx, FALSE);
    }
    g_assert_cmpint (mcp_session_get_state (MCP_SESSION (client)),
                      ==, MCP_SESSION_STATE_READY);

    /* Call echo. */
    loop = g_main_loop_new (mctx, FALSE);
    cc.loop = loop;
    json_builder_begin_object (ab);
    json_builder_set_member_name (ab, "text");
    json_builder_add_string_value (ab, "round-trip");
    json_builder_end_object (ab);
    root = json_builder_get_root (ab);
    args = json_object_ref (json_node_get_object (root));
    json_node_unref (root);

    mcp_client_call_tool_async (client, "echo", args, NULL,
                                 on_call_done, &cc);
    g_main_loop_run (loop);

    g_assert_null (cc.error);
    g_assert_nonnull (cc.result);
    {
        JsonArray *content = mcp_tool_result_get_content (cc.result);
        g_assert_nonnull (content);
        g_assert_cmpuint (json_array_get_length (content), >=, 1);
    }
    mcp_tool_result_unref (cc.result);

    g_main_context_pop_thread_default (mctx);
}

/* ── main ─────────────────────────────────────────────────────────── */

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/mcp/mux/construct-default-context",
                     test_construct_default_context);
    g_test_add_func ("/mcp/mux/construct-explicit-context",
                     test_construct_explicit_context);

    g_test_add_func ("/mcp/mux/send/routes-to-callback",
                     test_send_routes_to_callback);
    g_test_add_func ("/mcp/mux/send/destroy-called-on-finalize",
                     test_send_destroy_called_on_finalize);
    g_test_add_func ("/mcp/mux/send/destroy-runs-only-on-replacement",
                     test_send_destroy_runs_only_on_replacement);
    g_test_add_func ("/mcp/mux/send/fails-when-disconnected",
                     test_send_fails_when_disconnected);
    g_test_add_func ("/mcp/mux/send/fails-when-no-callback",
                     test_send_fails_when_no_callback);

    g_test_add_func ("/mcp/mux/dispatch/emits-message-received",
                     test_dispatch_emits_message_received);
    g_test_add_func ("/mcp/mux/dispatch/dropped-when-disconnected",
                     test_dispatch_dropped_when_disconnected);
    g_test_add_func ("/mcp/mux/dispatch/multiple-in-order",
                     test_dispatch_multiple_in_order);
    g_test_add_func ("/mcp/mux/dispatch/from-worker-thread",
                     test_dispatch_from_worker_thread);

    g_test_add_func ("/mcp/mux/state/changes-emitted",
                     test_state_changes_emitted);
    g_test_add_func ("/mcp/mux/state/connect-advances",
                     test_connect_advances_state);
    g_test_add_func ("/mcp/mux/state/connect-when-already-connected",
                     test_connect_when_already_connected);
    g_test_add_func ("/mcp/mux/state/disconnect-advances",
                     test_disconnect_advances_state);

    g_test_add_func ("/mcp/mux/error/from-host",
                     test_error_from_host);

    g_test_add_func ("/mcp/mux/guard/dispatch-null-self",
                     test_guard_dispatch_null_self);
    g_test_add_func ("/mcp/mux/guard/dispatch-null-frame",
                     test_guard_dispatch_null_frame);
    g_test_add_func ("/mcp/mux/guard/set-send-callback-null-self",
                     test_guard_set_send_callback_null_self);
    g_test_add_func ("/mcp/mux/guard/set-connected-null-self",
                     test_guard_set_connected_null_self);
    g_test_add_func ("/mcp/mux/guard/emit-error-null-error",
                     test_guard_emit_error_null_error);
    g_test_add_func ("/mcp/mux/guard/get-context-null",
                     test_guard_get_context_null);

    g_test_add_func ("/mcp/mux/integration/two-muxes-back-to-back",
                     test_two_muxes_back_to_back);
    g_test_add_func ("/mcp/mux/integration/full-mcp-stack",
                     test_full_mcp_stack_over_muxes);

    return g_test_run ();
}
