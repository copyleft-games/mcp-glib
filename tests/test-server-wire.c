/*
 * test-server-wire.c - Server tests that put replies on the wire
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Every other server test in this tree hands JsonNodes between a server
 * and a client in memory, so a reply that cannot be *serialised* passes
 * them all: json_node_new (JSON_NODE_OBJECT) answers
 * JSON_NODE_HOLDS_OBJECT() with TRUE, and /integration/ping was green
 * against a ping that killed the process the moment a real transport
 * tried to write it out.
 *
 * So these tests drive McpServer over McpMuxTransport and run every
 * outbound frame through JsonGenerator, which is what a stdio, HTTP or
 * WebSocket transport does.  They also feed the server the frames a
 * hostile or merely sloppy peer sends: members of the wrong JSON type,
 * where a presence check says "present" and the typed reader hands back
 * NULL.
 */

#define MCP_COMPILATION
#include "mcp.h"
#include "mcp-mux-transport.h"
#undef MCP_COMPILATION

/* ========================================================================== */
/* Fixture                                                                    */
/* ========================================================================== */

typedef struct
{
    GMainContext    *context;
    McpMuxTransport *transport;
    McpServer       *server;
    GPtrArray       *sent;        /* gchar * — serialised outbound frames */
} WireFixture;

/*
 * Serialise the frame here, in the send callback, because that is where a
 * real transport does it.  A test that only inspects the JsonNode tree
 * cannot tell an empty object from an object-typed node holding nothing.
 */
static void
on_send (McpMuxTransport *transport,
         JsonNode        *frame,
         gpointer         user_data)
{
    WireFixture *fixture = user_data;
    g_autoptr(JsonGenerator) generator = NULL;
    gchar *text;

    (void) transport;

    generator = json_generator_new ();
    json_generator_set_root (generator, frame);
    text = json_generator_to_data (generator, NULL);

    g_ptr_array_add (fixture->sent, text);
}

static void
wire_fixture_set_up (WireFixture   *fixture,
                     gconstpointer  user_data)
{
    (void) user_data;

    fixture->context = g_main_context_new ();
    g_main_context_push_thread_default (fixture->context);

    fixture->sent = g_ptr_array_new_with_free_func (g_free);
    fixture->transport = mcp_mux_transport_new (fixture->context);
    mcp_mux_transport_set_send_callback (fixture->transport, on_send, fixture, NULL);

    fixture->server = mcp_server_new ("wire-test", "1.0.0");
    mcp_server_set_transport (fixture->server, MCP_TRANSPORT (fixture->transport));
    mcp_server_start_async (fixture->server, NULL, NULL, NULL);

    mcp_mux_transport_set_connected (fixture->transport, TRUE);

    /* Let mcp_server_start_async()'s completion land.  It sets the session
     * to CONNECTING from a GTask callback, so a request dispatched before
     * that runs is answered and then has the session state it advanced
     * walked back underneath it. */
    {
        gint64 deadline = g_get_monotonic_time () + G_USEC_PER_SEC;

        while (mcp_session_get_state (MCP_SESSION (fixture->server))
                   != MCP_SESSION_STATE_CONNECTING
               && g_get_monotonic_time () < deadline)
        {
            g_main_context_iteration (fixture->context, FALSE);
        }
    }
}

static void
wire_fixture_tear_down (WireFixture   *fixture,
                        gconstpointer  user_data)
{
    (void) user_data;

    mcp_mux_transport_set_send_callback (fixture->transport, NULL, NULL, NULL);
    g_clear_object (&fixture->server);
    g_clear_object (&fixture->transport);
    g_ptr_array_unref (fixture->sent);

    g_main_context_pop_thread_default (fixture->context);
    g_main_context_unref (fixture->context);
}

/* Pump the fixture context until @count frames have gone out, or 2 s. */
static void
pump_for_frames (WireFixture *fixture,
                 guint        count)
{
    gint64 deadline = g_get_monotonic_time () + 2 * G_USEC_PER_SEC;

    while (fixture->sent->len < count && g_get_monotonic_time () < deadline)
    {
        g_main_context_iteration (fixture->context, FALSE);
    }
}

static void
dispatch_json (WireFixture *fixture,
               const gchar *json)
{
    g_autoptr(JsonParser) parser = json_parser_new ();
    g_autoptr(GError) error = NULL;

    g_assert_true (json_parser_load_from_data (parser, json, -1, &error));
    g_assert_no_error (error);

    mcp_mux_transport_dispatch_frame (fixture->transport,
                                      json_parser_get_root (parser));
}

/* The reply to request @id, parsed back from the bytes that were sent. */
static JsonObject *
find_frame_with_id (WireFixture  *fixture,
                    gint64        id,
                    JsonParser  **out_parser)
{
    guint i;

    for (i = 0; i < fixture->sent->len; i++)
    {
        JsonParser *parser = json_parser_new ();
        const gchar *text = g_ptr_array_index (fixture->sent, i);
        JsonObject *obj;

        if (!json_parser_load_from_data (parser, text, -1, NULL))
        {
            g_object_unref (parser);
            continue;
        }

        obj = json_node_get_object (json_parser_get_root (parser));
        if (obj != NULL && json_object_has_member (obj, "id") &&
            json_object_get_int_member (obj, "id") == id)
        {
            *out_parser = parser;
            return obj;
        }

        g_object_unref (parser);
    }

    return NULL;
}

/*
 * Pump until the reply to @id has been written, or 2 s.  Addressing the
 * reply by id rather than by position keeps the notifications a handler
 * may also emit from deciding what "the last frame" means.
 */
static JsonObject *
pump_for_reply (WireFixture  *fixture,
                gint64        id,
                JsonParser  **out_parser)
{
    gint64 deadline = g_get_monotonic_time () + 2 * G_USEC_PER_SEC;
    JsonObject *obj;

    obj = find_frame_with_id (fixture, id, out_parser);
    while (obj == NULL && g_get_monotonic_time () < deadline)
    {
        g_main_context_iteration (fixture->context, FALSE);
        obj = find_frame_with_id (fixture, id, out_parser);
    }

    return obj;
}

/* ========================================================================== */
/* Replies that must survive serialisation                                    */
/* ========================================================================== */

/*
 * handle_ping() built its reply with json_node_new (JSON_NODE_OBJECT),
 * which is an object-shaped hole rather than {} — so this test does not
 * fail an assertion without the fix, it takes the process down inside
 * json_object_get_members_internal().
 */
static void
test_ping_serialises (WireFixture   *fixture,
                      gconstpointer  user_data)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonObject *reply;

    (void) user_data;

    dispatch_json (fixture, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}");

    reply = pump_for_reply (fixture, 1, &parser);
    g_assert_nonnull (reply);
    g_assert_true (json_object_has_member (reply, "result"));
    g_assert_true (JSON_NODE_HOLDS_OBJECT (json_object_get_member (reply, "result")));
    g_assert_nonnull (json_object_get_object_member (reply, "result"));
}

static void
test_resources_subscribe_serialises (WireFixture   *fixture,
                                     gconstpointer  user_data)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonObject *reply;

    (void) user_data;

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"resources/subscribe\","
                   "\"params\":{\"uri\":\"file:///tmp/x\"}}");
    reply = pump_for_reply (fixture, 2, &parser);
    g_assert_nonnull (reply);
    g_assert_nonnull (json_object_get_object_member (reply, "result"));
}

static void
test_resources_unsubscribe_serialises (WireFixture   *fixture,
                                       gconstpointer  user_data)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonObject *reply;

    (void) user_data;

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"resources/unsubscribe\","
                   "\"params\":{\"uri\":\"file:///tmp/x\"}}");
    reply = pump_for_reply (fixture, 3, &parser);
    g_assert_nonnull (reply);
    g_assert_nonnull (json_object_get_object_member (reply, "result"));
}

static McpToolResult *
async_tool_handler (McpServer   *server,
                    McpTask     *task,
                    const gchar *name,
                    JsonObject  *arguments,
                    gpointer     user_data)
{
    (void) server;
    (void) task;
    (void) name;
    (void) arguments;
    (void) user_data;

    /* NULL means "still working", which is what leaves a task to cancel. */
    return NULL;
}

static void
test_tasks_cancel_serialises (WireFixture   *fixture,
                              gconstpointer  user_data)
{
    g_autoptr(McpTool) tool = NULL;
    g_autoptr(JsonParser) parser = NULL;
    JsonObject *reply;

    (void) user_data;

    tool = mcp_tool_new ("slow", "Never finishes");
    mcp_server_add_async_tool (fixture->server, tool, async_tool_handler, NULL, NULL);

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\","
                   "\"params\":{\"name\":\"slow\"}}");

    {
        g_autoptr(JsonParser) call_parser = NULL;
        g_assert_nonnull (pump_for_reply (fixture, 4, &call_parser));
    }

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tasks/cancel\","
                   "\"params\":{\"taskId\":\"task-1\"}}");

    reply = pump_for_reply (fixture, 5, &parser);
    g_assert_nonnull (reply);
    g_assert_nonnull (json_object_get_object_member (reply, "result"));
}

/* ========================================================================== */
/* Members of the wrong JSON type                                             */
/* ========================================================================== */

/*
 * {"params":{"name":123}} passed the has_member gate, and
 * json_object_get_string_member() handed back NULL for the number.  That
 * NULL went into g_hash_table_contains() on a g_str_hash table, which
 * dereferences it.
 */
static void
test_tools_call_non_string_name (WireFixture   *fixture,
                                 gconstpointer  user_data)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonObject *reply;

    (void) user_data;

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\","
                   "\"params\":{\"name\":123}}");

    reply = pump_for_reply (fixture, 6, &parser);
    g_assert_nonnull (reply);
    g_assert_true (json_object_has_member (reply, "error"));
}

static void
test_resources_read_non_string_uri (WireFixture   *fixture,
                                    gconstpointer  user_data)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonObject *reply;

    (void) user_data;

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"resources/read\","
                   "\"params\":{\"uri\":[1,2]}}");
    reply = pump_for_reply (fixture, 7, &parser);
    g_assert_nonnull (reply);
    g_assert_true (json_object_has_member (reply, "error"));
}

static void
test_prompts_get_non_string_name (WireFixture   *fixture,
                                  gconstpointer  user_data)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonObject *reply;

    (void) user_data;

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"prompts/get\","
                   "\"params\":{\"name\":{\"a\":1}}}");
    reply = pump_for_reply (fixture, 8, &parser);
    g_assert_nonnull (reply);
    g_assert_true (json_object_has_member (reply, "error"));
}

static void
test_resources_subscribe_non_string_uri (WireFixture   *fixture,
                                         gconstpointer  user_data)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonObject *reply;

    (void) user_data;

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"resources/subscribe\","
                   "\"params\":{\"uri\":false}}");
    reply = pump_for_reply (fixture, 9, &parser);
    g_assert_nonnull (reply);
    g_assert_true (json_object_has_member (reply, "error"));
}

static void
test_tasks_get_non_string_id (WireFixture   *fixture,
                              gconstpointer  user_data)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonObject *reply;

    (void) user_data;

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"tasks/get\","
                   "\"params\":{\"taskId\":7}}");
    reply = pump_for_reply (fixture, 10, &parser);
    g_assert_nonnull (reply);
    g_assert_true (json_object_has_member (reply, "error"));
}

/*
 * completion/complete reads ref.type, ref.name, ref.uri and
 * argument.value, and every one of them was a presence check followed by
 * a typed read.
 */
static void
test_completion_non_string_ref_type (WireFixture   *fixture,
                                     gconstpointer  user_data)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonObject *reply;

    (void) user_data;

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"completion/complete\","
                   "\"params\":{\"ref\":{\"type\":42},"
                   "\"argument\":{\"name\":\"x\",\"value\":\"y\"}}}");
    reply = pump_for_reply (fixture, 11, &parser);
    g_assert_nonnull (reply);
    g_assert_true (json_object_has_member (reply, "error"));
}

/*
 * A hostile clientInfo used to reach mcp_implementation_new() with a NULL
 * name, which trips a g_return_val_if_fail — fatal under GTest.
 */
static void
test_initialize_non_string_client_info (WireFixture   *fixture,
                                        gconstpointer  user_data)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonObject *reply;

    (void) user_data;

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"initialize\","
                   "\"params\":{\"clientInfo\":{\"name\":5,\"version\":\"1\"}}}");
    reply = pump_for_reply (fixture, 12, &parser);
    g_assert_nonnull (reply);
    g_assert_true (json_object_has_member (reply, "result"));
}

/* ========================================================================== */
/* Cancellation must not reach across id spaces                               */
/* ========================================================================== */

typedef struct
{
    gboolean  finished;
    GError   *error;
} SamplingOutcome;

static void
sampling_cb (GObject      *source,
             GAsyncResult *result,
             gpointer      user_data)
{
    SamplingOutcome *outcome = user_data;

    mcp_server_request_sampling_finish (MCP_SERVER (source), result, &outcome->error);
    outcome->finished = TRUE;
}

/*
 * "notifications/cancelled" carries the id of a *client-initiated*
 * request.  Looking it up in the server's own outbound request table
 * cancels whatever server request happens to wear the same number.
 */
static void
test_cancelled_does_not_kill_server_request (WireFixture   *fixture,
                                             gconstpointer  user_data)
{
    McpSamplingMessage *message;
    GList *messages = NULL;
    SamplingOutcome outcome = { FALSE, NULL };
    gint64 deadline;

    (void) user_data;

    /* Give the server a client so it is willing to send its own request. */
    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\","
                   "\"params\":{\"capabilities\":{\"sampling\":{}},"
                   "\"clientInfo\":{\"name\":\"c\",\"version\":\"1\"}}}");
    {
        g_autoptr(JsonParser) init_parser = NULL;
        g_assert_nonnull (pump_for_reply (fixture, 1, &init_parser));
    }

    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
    pump_for_frames (fixture, 2);

    message = mcp_sampling_message_new (MCP_ROLE_USER);
    mcp_sampling_message_add_text (message, "hello");
    messages = g_list_append (messages, message);

    mcp_server_request_sampling_async (fixture->server, messages, NULL, NULL, 16,
                                       NULL, sampling_cb, &outcome);
    pump_for_frames (fixture, 2);

    /* The request must actually be in flight, or the test proves nothing
     * about what a cancellation can reach. */
    g_assert_false (outcome.finished);

    /* The client cancels *its own* request 1 — the initialize it already
     * got an answer to.  Nothing about the server's outbound request. */
    dispatch_json (fixture,
                   "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/cancelled\","
                   "\"params\":{\"requestId\":\"1\"}}");

    deadline = g_get_monotonic_time () + G_USEC_PER_SEC / 2;
    while (!outcome.finished && g_get_monotonic_time () < deadline)
    {
        g_main_context_iteration (fixture->context, FALSE);
    }

    g_list_free_full (messages, (GDestroyNotify) mcp_sampling_message_unref);

    g_assert_false (outcome.finished);
    g_assert_no_error (outcome.error);
}

/* ========================================================================== */
/* Main                                                                       */
/* ========================================================================== */

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

#define WIRE_TEST(path, func) \
    g_test_add (path, WireFixture, NULL, wire_fixture_set_up, func, wire_fixture_tear_down)

    WIRE_TEST ("/mcp/server-wire/ping-serialises", test_ping_serialises);
    WIRE_TEST ("/mcp/server-wire/resources-subscribe-serialises",
               test_resources_subscribe_serialises);
    WIRE_TEST ("/mcp/server-wire/resources-unsubscribe-serialises",
               test_resources_unsubscribe_serialises);
    WIRE_TEST ("/mcp/server-wire/tasks-cancel-serialises",
               test_tasks_cancel_serialises);

    WIRE_TEST ("/mcp/server-wire/tools-call-non-string-name",
               test_tools_call_non_string_name);
    WIRE_TEST ("/mcp/server-wire/resources-read-non-string-uri",
               test_resources_read_non_string_uri);
    WIRE_TEST ("/mcp/server-wire/prompts-get-non-string-name",
               test_prompts_get_non_string_name);
    WIRE_TEST ("/mcp/server-wire/resources-subscribe-non-string-uri",
               test_resources_subscribe_non_string_uri);
    WIRE_TEST ("/mcp/server-wire/tasks-get-non-string-id",
               test_tasks_get_non_string_id);
    WIRE_TEST ("/mcp/server-wire/completion-non-string-ref-type",
               test_completion_non_string_ref_type);
    WIRE_TEST ("/mcp/server-wire/initialize-non-string-client-info",
               test_initialize_non_string_client_info);

    WIRE_TEST ("/mcp/server-wire/cancelled-does-not-kill-server-request",
               test_cancelled_does_not_kill_server_request);

#undef WIRE_TEST

    return g_test_run ();
}
