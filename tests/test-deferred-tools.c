/*
 * test-deferred-tools.c - Deferred tool calls (McpToolCall) end to end
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * A server and a client joined by a Unix socket pair, each on an
 * McpStdioTransport, so every frame is really serialised.  A deferred
 * tool's handler keeps its McpToolCall and the test completes it later:
 * the client's tools/call stays open meanwhile, progress notifications
 * reach McpClient::progress with the client's token, an answer from
 * inside the handler works too, a JSON-RPC error comes back as one, and
 * notifications/cancelled or the connection closing cancels the call
 * (its signal, its cancellable) with nothing answered afterwards.
 */

#include "mcp.h"

#include <sys/socket.h>
#include <gio/gunixsocketaddress.h>

typedef struct
{
    McpServer      *server;
    McpClient      *client;
    McpTransport   *client_transport;
    McpTransport   *server_transport;
    McpToolCall    *call;          /* the deferred handler's, kept */
    gboolean        answer_now;    /* the handler answers inside itself */
    guint           progress_seen;
    gchar          *progress_token;
    gdouble         progress_total;
    gchar          *progress_message;
    guint           cancelled_seen;
} Fixture;

typedef struct
{
    GAsyncResult *result;
} Wait;

static void
on_done (GObject      *source,
         GAsyncResult *result,
         gpointer      user_data)
{
    (void) source;
    ((Wait *) user_data)->result = g_object_ref (result);
}

/* iterates the default context until @wait has a result, or @us passed */
static gboolean
spin (Wait   *wait,
      gint64  us)
{
    gint64 deadline = g_get_monotonic_time () + us;

    while (wait->result == NULL && g_get_monotonic_time () < deadline)
    {
        g_main_context_iteration (NULL, FALSE);
    }
    return wait->result != NULL;
}

static void
spin_for (gint64 us)
{
    gint64 deadline = g_get_monotonic_time () + us;

    while (g_get_monotonic_time () < deadline)
    {
        g_main_context_iteration (NULL, FALSE);
    }
}

static void
on_call_cancelled (McpToolCall *call,
                   gpointer     user_data)
{
    (void) call;
    ((Fixture *) user_data)->cancelled_seen++;
}

static void
slow_handler (McpServer   *server,
              McpToolCall *call,
              gpointer     user_data)
{
    Fixture *fx = user_data;

    (void) server;
    g_assert_cmpstr (mcp_tool_call_get_name (call), ==, "slow");
    if (fx->answer_now)
    {
        g_autoptr(McpToolResult) result = mcp_tool_result_new (FALSE);

        mcp_tool_result_add_text (result, "at once");
        mcp_tool_call_return_result (call, result);
        return;
    }
    g_signal_connect (call, "cancelled", G_CALLBACK (on_call_cancelled), fx);
    g_clear_object (&fx->call);
    fx->call = g_object_ref (call);
}

static void
on_progress (McpClient   *client,
             const gchar *token,
             gdouble      progress,
             gdouble      total,
             const gchar *message,
             gpointer     user_data)
{
    Fixture *fx = user_data;

    (void) client;
    fx->progress_seen++;
    g_free (fx->progress_token);
    fx->progress_token = g_strdup (token);
    fx->progress_total = total;
    g_free (fx->progress_message);
    fx->progress_message = g_strdup (message);
    g_assert_cmpfloat (progress, ==, (gdouble) fx->progress_seen);
}

/* an McpStdioTransport over one end of a socket pair */
static McpTransport *
transport_for_fd (gint fd)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GSocket) socket = NULL;
    g_autoptr(GSocketConnection) connection = NULL;
    McpStdioTransport *transport;

    socket = g_socket_new_from_fd (fd, &error);
    g_assert_no_error (error);
    connection = g_socket_connection_factory_create_connection (socket);
    transport = mcp_stdio_transport_new_with_streams (
        g_io_stream_get_input_stream (G_IO_STREAM (connection)),
        g_io_stream_get_output_stream (G_IO_STREAM (connection)));
    /* the streams keep the connection alive through the transport */
    g_object_set_data_full (G_OBJECT (transport), "connection",
                            g_object_ref (connection), g_object_unref);
    return MCP_TRANSPORT (transport);
}

static void
fixture_setup (Fixture       *fx,
               gconstpointer  data)
{
    g_autoptr(McpTool) tool = NULL;
    g_autoptr(GError) error = NULL;
    gint fds[2];
    Wait wait = { NULL };

    (void) data;
    g_assert_cmpint (socketpair (AF_UNIX, SOCK_STREAM, 0, fds), ==, 0);
    fx->server_transport = transport_for_fd (fds[0]);
    fx->client_transport = transport_for_fd (fds[1]);

    fx->server = mcp_server_new ("deferred", "1.0");
    tool = mcp_tool_new ("slow", "Answers when the test says so");
    mcp_server_add_deferred_tool (fx->server, tool, slow_handler, fx, NULL);
    mcp_server_set_transport (fx->server, fx->server_transport);
    mcp_server_start_async (fx->server, NULL, NULL, NULL);

    fx->client = mcp_client_new ("test", "1.0");
    g_signal_connect (fx->client, "progress", G_CALLBACK (on_progress), fx);
    mcp_client_set_transport (fx->client, fx->client_transport);
    mcp_client_connect_async (fx->client, NULL, on_done, &wait);
    g_assert_true (spin (&wait, 5 * G_USEC_PER_SEC));
    g_assert_true (mcp_client_connect_finish (fx->client, wait.result, &error));
    g_assert_no_error (error);
    g_object_unref (wait.result);
}

static void
fixture_teardown (Fixture       *fx,
                  gconstpointer  data)
{
    (void) data;
    g_clear_object (&fx->call);
    g_clear_object (&fx->client);
    mcp_server_stop (fx->server);
    spin_for (G_USEC_PER_SEC / 20);
    g_clear_object (&fx->server);
    g_clear_object (&fx->client_transport);
    g_clear_object (&fx->server_transport);
    g_free (fx->progress_token);
    g_free (fx->progress_message);
}

/* the client's call stays open until the handler answers, with progress */
static void
test_answer_later (Fixture       *fx,
                   gconstpointer  data)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(McpToolResult) result = NULL;
    g_autoptr(McpToolResult) answer = NULL;
    JsonArray *content;
    Wait wait = { NULL };

    (void) data;
    mcp_client_call_tool_with_progress_async (fx->client, "slow", NULL, "walk-1", NULL, on_done, &wait);
    g_assert_false (spin (&wait, G_USEC_PER_SEC / 5));
    g_assert_nonnull (fx->call);
    g_assert_true (mcp_tool_call_has_progress_token (fx->call));
    g_assert_cmpuint (mcp_server_get_pending_tool_call_count (fx->server), ==, 1);
    g_assert_false (mcp_tool_call_is_completed (fx->call));

    /* progress, with the client's own token */
    mcp_tool_call_send_progress (fx->call, 1.0, 3.0, "a third of the way");
    mcp_tool_call_send_progress (fx->call, 2.0, 0.0, NULL);
    spin_for (G_USEC_PER_SEC / 10);
    g_assert_cmpuint (fx->progress_seen, ==, 2);
    g_assert_cmpstr (fx->progress_token, ==, "walk-1");
    g_assert_cmpfloat (fx->progress_total, ==, -1.0);
    g_assert_null (fx->progress_message);
    g_assert_false (spin (&wait, G_USEC_PER_SEC / 10));

    /* the answer */
    answer = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_text (answer, "arrived");
    mcp_tool_call_return_result (fx->call, answer);
    g_assert_true (mcp_tool_call_is_completed (fx->call));
    g_assert_cmpuint (mcp_server_get_pending_tool_call_count (fx->server), ==, 0);
    g_assert_true (spin (&wait, 5 * G_USEC_PER_SEC));
    result = mcp_client_call_tool_finish (fx->client, wait.result, &error);
    g_object_unref (wait.result);
    g_assert_no_error (error);
    content = mcp_tool_result_get_content (result);
    g_assert_cmpstr (json_object_get_string_member (json_array_get_object_element (content, 0), "text"),
                     ==, "arrived");

    /* a second return, and progress after the end, are ignored */
    mcp_tool_call_return_result (fx->call, answer);
    mcp_tool_call_send_progress (fx->call, 3.0, 3.0, NULL);
    spin_for (G_USEC_PER_SEC / 10);
    g_assert_cmpuint (fx->progress_seen, ==, 2);
}

/* a handler may answer inside itself; no token means no progress */
static void
test_answer_at_once (Fixture       *fx,
                     gconstpointer  data)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(McpToolResult) result = NULL;
    Wait wait = { NULL };

    (void) data;
    fx->answer_now = TRUE;
    mcp_client_call_tool_async (fx->client, "slow", NULL, NULL, on_done, &wait);
    g_assert_true (spin (&wait, 5 * G_USEC_PER_SEC));
    result = mcp_client_call_tool_finish (fx->client, wait.result, &error);
    g_object_unref (wait.result);
    g_assert_no_error (error);
    g_assert_cmpuint (mcp_server_get_pending_tool_call_count (fx->server), ==, 0);
    g_assert_cmpstr (json_object_get_string_member (
                         json_array_get_object_element (mcp_tool_result_get_content (result), 0), "text"),
                     ==, "at once");
}

static void
test_error (Fixture       *fx,
            gconstpointer  data)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(McpToolResult) result = NULL;
    Wait wait = { NULL };

    (void) data;
    mcp_client_call_tool_async (fx->client, "slow", NULL, NULL, on_done, &wait);
    g_assert_false (spin (&wait, G_USEC_PER_SEC / 5));
    g_assert_false (mcp_tool_call_has_progress_token (fx->call));
    mcp_tool_call_send_progress (fx->call, 1.0, 2.0, "nobody asked");
    mcp_tool_call_return_error (fx->call, MCP_ERROR_INVALID_PARAMS, "no such place");
    g_assert_true (spin (&wait, 5 * G_USEC_PER_SEC));
    result = mcp_client_call_tool_finish (fx->client, wait.result, &error);
    g_object_unref (wait.result);
    g_assert_null (result);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_cmpuint (fx->progress_seen, ==, 0);
}

/* notifications/cancelled: the call is cancelled and never answered */
static void
test_cancelled (Fixture       *fx,
                gconstpointer  data)
{
    g_autoptr(McpNotification) cancel = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) message = NULL;
    g_autoptr(McpToolResult) late = NULL;
    Wait wait = { NULL };

    (void) data;
    mcp_client_call_tool_async (fx->client, "slow", NULL, NULL, on_done, &wait);
    g_assert_false (spin (&wait, G_USEC_PER_SEC / 5));
    g_assert_nonnull (fx->call);

    /* the client cancels by the request's id */
    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "requestId");
    json_builder_add_string_value (builder, mcp_tool_call_get_request_id (fx->call));
    json_builder_set_member_name (builder, "reason");
    json_builder_add_string_value (builder, "the player moved");
    json_builder_end_object (builder);
    cancel = mcp_notification_new ("notifications/cancelled");
    mcp_notification_set_params (cancel, json_builder_get_root (builder));
    message = mcp_message_to_json (MCP_MESSAGE (cancel));
    mcp_transport_send_message_async (fx->client_transport, message, NULL, NULL, NULL);
    spin_for (G_USEC_PER_SEC / 5);

    g_assert_cmpuint (fx->cancelled_seen, ==, 1);
    g_assert_true (mcp_tool_call_is_cancelled (fx->call));
    g_assert_true (g_cancellable_is_cancelled (mcp_tool_call_get_cancellable (fx->call)));
    g_assert_cmpuint (mcp_server_get_pending_tool_call_count (fx->server), ==, 0);

    /* an answer after the cancellation is not sent */
    late = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_text (late, "too late");
    mcp_tool_call_return_result (fx->call, late);
    g_assert_false (spin (&wait, G_USEC_PER_SEC / 5));
    /* the client's call is still open; stopping the client ends it */
    mcp_transport_disconnect_async (fx->client_transport, NULL, NULL, NULL);
    g_assert_true (spin (&wait, 5 * G_USEC_PER_SEC));
    g_object_unref (wait.result);
}

/* the connection closing cancels the calls in flight */
static void
test_disconnect (Fixture       *fx,
                 gconstpointer  data)
{
    Wait wait = { NULL };

    (void) data;
    mcp_client_call_tool_async (fx->client, "slow", NULL, NULL, on_done, &wait);
    g_assert_false (spin (&wait, G_USEC_PER_SEC / 5));
    g_assert_nonnull (fx->call);
    mcp_server_stop (fx->server);
    spin_for (G_USEC_PER_SEC / 10);
    g_assert_cmpuint (fx->cancelled_seen, ==, 1);
    g_assert_true (mcp_tool_call_is_cancelled (fx->call));
    g_assert_cmpuint (mcp_server_get_pending_tool_call_count (fx->server), ==, 0);
    /* the client's own end closing ends its call */
    mcp_transport_disconnect_async (fx->client_transport, NULL, NULL, NULL);
    g_assert_true (spin (&wait, 5 * G_USEC_PER_SEC));
    g_object_unref (wait.result);
}

/* registering the name again as a plain tool drops the deferred dispatch */
static void
test_replace (Fixture       *fx,
              gconstpointer  data)
{
    g_autoptr(McpTool) plain = NULL;

    (void) data;
    plain = mcp_tool_new ("slow", "Now plain");
    mcp_server_add_tool (fx->server, plain, NULL, NULL, NULL);
    {
        g_autoptr(GError) error = NULL;
        g_autoptr(McpToolResult) result = NULL;
        Wait wait = { NULL };

        mcp_client_call_tool_async (fx->client, "slow", NULL, NULL, on_done, &wait);
        g_assert_true (spin (&wait, 5 * G_USEC_PER_SEC));
        result = mcp_client_call_tool_finish (fx->client, wait.result, &error);
        g_object_unref (wait.result);
        g_assert_no_error (error);
        g_assert_null (fx->call);
    }
}

int
main (int    argc,
      char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add ("/deferred/answer-later", Fixture, NULL, fixture_setup, test_answer_later, fixture_teardown);
    g_test_add ("/deferred/answer-at-once", Fixture, NULL, fixture_setup, test_answer_at_once, fixture_teardown);
    g_test_add ("/deferred/error", Fixture, NULL, fixture_setup, test_error, fixture_teardown);
    g_test_add ("/deferred/cancelled", Fixture, NULL, fixture_setup, test_cancelled, fixture_teardown);
    g_test_add ("/deferred/disconnect", Fixture, NULL, fixture_setup, test_disconnect, fixture_teardown);
    g_test_add ("/deferred/replace", Fixture, NULL, fixture_setup, test_replace, fixture_teardown);

    return g_test_run ();
}
