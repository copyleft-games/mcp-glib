/*
 * test-transport-mock.c - Transport tests with mock implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define MCP_COMPILATION
#include <mcp/mcp-transport.h>
#include <mcp/mcp-stdio-transport.h>
#include <mcp/mcp-error.h>
#undef MCP_COMPILATION

/* ========================================================================== */
/* Mock Transport Implementation                                              */
/* ========================================================================== */

#define TEST_TYPE_MOCK_TRANSPORT (test_mock_transport_get_type ())
G_DECLARE_FINAL_TYPE (TestMockTransport, test_mock_transport, TEST, MOCK_TRANSPORT, GObject)

struct _TestMockTransport
{
    GObject parent_instance;

    McpTransportState state;

    /* Queue of messages to "receive" */
    GQueue *receive_queue;

    /* Captured sent messages */
    GQueue *sent_queue;

    /* Control flags */
    gboolean fail_connect;
    gboolean fail_send;
};

static void test_mock_transport_iface_init (McpTransportInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TestMockTransport, test_mock_transport, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MCP_TYPE_TRANSPORT,
                                                test_mock_transport_iface_init))

static void
test_mock_transport_finalize (GObject *object)
{
    TestMockTransport *self = TEST_MOCK_TRANSPORT (object);

    g_queue_free_full (self->receive_queue, (GDestroyNotify) json_node_unref);
    g_queue_free_full (self->sent_queue, (GDestroyNotify) json_node_unref);

    G_OBJECT_CLASS (test_mock_transport_parent_class)->finalize (object);
}

static void
test_mock_transport_class_init (TestMockTransportClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = test_mock_transport_finalize;
}

static void
test_mock_transport_init (TestMockTransport *self)
{
    self->state = MCP_TRANSPORT_STATE_DISCONNECTED;
    self->receive_queue = g_queue_new ();
    self->sent_queue = g_queue_new ();
}

static McpTransportState
mock_get_state (McpTransport *transport)
{
    TestMockTransport *self = TEST_MOCK_TRANSPORT (transport);
    return self->state;
}

static void
mock_connect_async (McpTransport        *transport,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
    TestMockTransport *self = TEST_MOCK_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;

    task = g_task_new (self, cancellable, callback, user_data);

    if (self->fail_connect)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_CONNECTION_CLOSED,
                                 "Mock connection failure");
        return;
    }

    self->state = MCP_TRANSPORT_STATE_CONNECTED;
    mcp_transport_emit_state_changed (transport,
                                      MCP_TRANSPORT_STATE_DISCONNECTED,
                                      MCP_TRANSPORT_STATE_CONNECTED);
    g_task_return_boolean (task, TRUE);
}

static gboolean
mock_connect_finish (McpTransport  *transport,
                     GAsyncResult  *result,
                     GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mock_disconnect_async (McpTransport        *transport,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
    TestMockTransport *self = TEST_MOCK_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;
    McpTransportState old_state;

    task = g_task_new (self, cancellable, callback, user_data);

    old_state = self->state;
    self->state = MCP_TRANSPORT_STATE_DISCONNECTED;
    mcp_transport_emit_state_changed (transport, old_state,
                                      MCP_TRANSPORT_STATE_DISCONNECTED);
    g_task_return_boolean (task, TRUE);
}

static gboolean
mock_disconnect_finish (McpTransport  *transport,
                        GAsyncResult  *result,
                        GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mock_send_message_async (McpTransport        *transport,
                         JsonNode            *message,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
    TestMockTransport *self = TEST_MOCK_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;

    task = g_task_new (self, cancellable, callback, user_data);

    if (self->fail_send)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                 "Mock send failure");
        return;
    }

    if (self->state != MCP_TRANSPORT_STATE_CONNECTED)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_CONNECTION_CLOSED,
                                 "Not connected");
        return;
    }

    /* Capture the sent message */
    g_queue_push_tail (self->sent_queue, json_node_copy (message));

    g_task_return_boolean (task, TRUE);
}

static gboolean
mock_send_message_finish (McpTransport  *transport,
                          GAsyncResult  *result,
                          GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
test_mock_transport_iface_init (McpTransportInterface *iface)
{
    iface->get_state = mock_get_state;
    iface->connect_async = mock_connect_async;
    iface->connect_finish = mock_connect_finish;
    iface->disconnect_async = mock_disconnect_async;
    iface->disconnect_finish = mock_disconnect_finish;
    iface->send_message_async = mock_send_message_async;
    iface->send_message_finish = mock_send_message_finish;
}

/* Mock transport helper functions */

static TestMockTransport *
test_mock_transport_new (void)
{
    return g_object_new (TEST_TYPE_MOCK_TRANSPORT, NULL);
}

static void
test_mock_transport_queue_receive (TestMockTransport *self,
                                   JsonNode          *message)
{
    g_queue_push_tail (self->receive_queue, json_node_copy (message));
}

static void
test_mock_transport_simulate_receive (TestMockTransport *self)
{
    JsonNode *message;

    message = g_queue_pop_head (self->receive_queue);
    if (message != NULL)
    {
        mcp_transport_emit_message_received (MCP_TRANSPORT (self), message);
        json_node_unref (message);
    }
}

static JsonNode *
test_mock_transport_pop_sent (TestMockTransport *self)
{
    return g_queue_pop_head (self->sent_queue);
}

static guint
test_mock_transport_get_sent_count (TestMockTransport *self)
{
    return g_queue_get_length (self->sent_queue);
}

/* ========================================================================== */
/* Test Fixtures                                                              */
/* ========================================================================== */

typedef struct
{
    TestMockTransport *transport;
    GMainLoop         *loop;
    gboolean           callback_called;
    gboolean           success;
    GError            *error;
} TransportFixture;

static void
transport_fixture_setup (TransportFixture *fixture,
                         gconstpointer     user_data)
{
    fixture->transport = test_mock_transport_new ();
    fixture->loop = g_main_loop_new (NULL, FALSE);
    fixture->callback_called = FALSE;
    fixture->success = FALSE;
    fixture->error = NULL;
}

static void
transport_fixture_teardown (TransportFixture *fixture,
                            gconstpointer     user_data)
{
    g_clear_object (&fixture->transport);
    g_clear_pointer (&fixture->loop, g_main_loop_unref);
    g_clear_error (&fixture->error);
}

/* ========================================================================== */
/* Transport Interface Tests                                                  */
/* ========================================================================== */

static void
test_transport_initial_state (TransportFixture *fixture,
                              gconstpointer     user_data)
{
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (fixture->transport)),
                     ==, MCP_TRANSPORT_STATE_DISCONNECTED);
    g_assert_false (mcp_transport_is_connected (MCP_TRANSPORT (fixture->transport)));
}

static void
connect_cb (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
    TransportFixture *fixture = user_data;

    fixture->callback_called = TRUE;
    fixture->success = mcp_transport_connect_finish (MCP_TRANSPORT (source),
                                                     result,
                                                     &fixture->error);
    g_main_loop_quit (fixture->loop);
}

static void
test_transport_connect (TransportFixture *fixture,
                        gconstpointer     user_data)
{
    mcp_transport_connect_async (MCP_TRANSPORT (fixture->transport),
                                 NULL, connect_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (fixture->transport)),
                     ==, MCP_TRANSPORT_STATE_CONNECTED);
    g_assert_true (mcp_transport_is_connected (MCP_TRANSPORT (fixture->transport)));
}

static void
test_transport_connect_failure (TransportFixture *fixture,
                                gconstpointer     user_data)
{
    fixture->transport->fail_connect = TRUE;

    mcp_transport_connect_async (MCP_TRANSPORT (fixture->transport),
                                 NULL, connect_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_false (fixture->success);
    g_assert_error (fixture->error, MCP_ERROR, MCP_ERROR_CONNECTION_CLOSED);
}

static void
disconnect_cb (GObject      *source,
               GAsyncResult *result,
               gpointer      user_data)
{
    TransportFixture *fixture = user_data;

    fixture->callback_called = TRUE;
    fixture->success = mcp_transport_disconnect_finish (MCP_TRANSPORT (source),
                                                        result,
                                                        &fixture->error);
    g_main_loop_quit (fixture->loop);
}

static void
test_transport_disconnect (TransportFixture *fixture,
                           gconstpointer     user_data)
{
    /* First connect */
    mcp_transport_connect_async (MCP_TRANSPORT (fixture->transport),
                                 NULL, connect_cb, fixture);
    g_main_loop_run (fixture->loop);
    g_assert_true (fixture->success);

    /* Reset fixture state */
    fixture->callback_called = FALSE;
    fixture->success = FALSE;

    /* Now disconnect */
    mcp_transport_disconnect_async (MCP_TRANSPORT (fixture->transport),
                                    NULL, disconnect_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (fixture->transport)),
                     ==, MCP_TRANSPORT_STATE_DISCONNECTED);
}

static void
send_cb (GObject      *source,
         GAsyncResult *result,
         gpointer      user_data)
{
    TransportFixture *fixture = user_data;

    fixture->callback_called = TRUE;
    fixture->success = mcp_transport_send_message_finish (MCP_TRANSPORT (source),
                                                          result,
                                                          &fixture->error);
    g_main_loop_quit (fixture->loop);
}

static void
test_transport_send_message (TransportFixture *fixture,
                             gconstpointer     user_data)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) message = NULL;
    g_autoptr(JsonNode) sent = NULL;

    /* Connect first */
    mcp_transport_connect_async (MCP_TRANSPORT (fixture->transport),
                                 NULL, connect_cb, fixture);
    g_main_loop_run (fixture->loop);
    g_assert_true (fixture->success);

    /* Reset fixture state */
    fixture->callback_called = FALSE;
    fixture->success = FALSE;

    /* Build a test message */
    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "jsonrpc");
    json_builder_add_string_value (builder, "2.0");
    json_builder_set_member_name (builder, "method");
    json_builder_add_string_value (builder, "test");
    json_builder_set_member_name (builder, "id");
    json_builder_add_string_value (builder, "1");
    json_builder_end_object (builder);
    message = json_builder_get_root (builder);

    /* Send the message */
    mcp_transport_send_message_async (MCP_TRANSPORT (fixture->transport),
                                      message, NULL, send_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);

    /* Verify the message was captured */
    g_assert_cmpuint (test_mock_transport_get_sent_count (fixture->transport), ==, 1);

    sent = test_mock_transport_pop_sent (fixture->transport);
    g_assert_nonnull (sent);

    /* Verify content */
    {
        JsonObject *obj = json_node_get_object (sent);
        g_assert_cmpstr (json_object_get_string_member (obj, "jsonrpc"), ==, "2.0");
        g_assert_cmpstr (json_object_get_string_member (obj, "method"), ==, "test");
        g_assert_cmpstr (json_object_get_string_member (obj, "id"), ==, "1");
    }
}

static void
test_transport_send_not_connected (TransportFixture *fixture,
                                   gconstpointer     user_data)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) message = NULL;

    /* Build a test message */
    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "test");
    json_builder_add_string_value (builder, "value");
    json_builder_end_object (builder);
    message = json_builder_get_root (builder);

    /* Try to send without connecting */
    mcp_transport_send_message_async (MCP_TRANSPORT (fixture->transport),
                                      message, NULL, send_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_false (fixture->success);
    g_assert_error (fixture->error, MCP_ERROR, MCP_ERROR_CONNECTION_CLOSED);
}

/* State change signal test */

static void
state_changed_cb (McpTransport *transport,
                  gint          old_state,
                  gint          new_state,
                  gpointer      user_data)
{
    gint *call_count = user_data;
    (*call_count)++;
}

static void
test_transport_state_signal (TransportFixture *fixture,
                             gconstpointer     user_data)
{
    gint call_count = 0;

    g_signal_connect (fixture->transport, "state-changed",
                      G_CALLBACK (state_changed_cb), &call_count);

    /* Connect */
    mcp_transport_connect_async (MCP_TRANSPORT (fixture->transport),
                                 NULL, connect_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_cmpint (call_count, ==, 1);

    /* Reset and disconnect */
    fixture->callback_called = FALSE;
    mcp_transport_disconnect_async (MCP_TRANSPORT (fixture->transport),
                                    NULL, disconnect_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_cmpint (call_count, ==, 2);
}

/* Message received signal test */

static void
message_received_cb (McpTransport *transport,
                     JsonNode     *message,
                     gpointer      user_data)
{
    JsonNode **received = user_data;
    *received = json_node_copy (message);
}

static void
test_transport_message_signal (TransportFixture *fixture,
                               gconstpointer     user_data)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) message = NULL;
    g_autoptr(JsonNode) received = NULL;

    /* Build a test message */
    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "test");
    json_builder_add_string_value (builder, "received_value");
    json_builder_end_object (builder);
    message = json_builder_get_root (builder);

    /* Connect signal */
    g_signal_connect (fixture->transport, "message-received",
                      G_CALLBACK (message_received_cb), &received);

    /* Queue and simulate receive */
    test_mock_transport_queue_receive (fixture->transport, message);
    test_mock_transport_simulate_receive (fixture->transport);

    g_assert_nonnull (received);

    /* Verify content */
    {
        JsonObject *obj = json_node_get_object (received);
        g_assert_cmpstr (json_object_get_string_member (obj, "test"), ==, "received_value");
    }
}

/* Error signal test */

static void
error_cb (McpTransport *transport,
          GError       *error,
          gpointer      user_data)
{
    GError **received_error = user_data;
    *received_error = g_error_copy (error);
}

static void
test_transport_error_signal (TransportFixture *fixture,
                             gconstpointer     user_data)
{
    g_autoptr(GError) received_error = NULL;
    g_autoptr(GError) test_error = NULL;

    /* Connect signal */
    g_signal_connect (fixture->transport, "error",
                      G_CALLBACK (error_cb), &received_error);

    /* Emit an error */
    test_error = g_error_new (MCP_ERROR, MCP_ERROR_PARSE_ERROR, "Test error");
    mcp_transport_emit_error (MCP_TRANSPORT (fixture->transport), test_error);

    g_assert_nonnull (received_error);
    g_assert_error (received_error, MCP_ERROR, MCP_ERROR_PARSE_ERROR);
    g_assert_cmpstr (received_error->message, ==, "Test error");
}

/* ========================================================================== */
/* Stdio Transport Tests                                                      */
/* ========================================================================== */

static void
test_stdio_transport_new (void)
{
    g_autoptr(McpStdioTransport) transport = NULL;

    transport = mcp_stdio_transport_new ();
    g_assert_nonnull (transport);
    g_assert_true (MCP_IS_STDIO_TRANSPORT (transport));
    g_assert_true (MCP_IS_TRANSPORT (transport));

    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (transport)),
                     ==, MCP_TRANSPORT_STATE_DISCONNECTED);
    g_assert_null (mcp_stdio_transport_get_subprocess (transport));
    g_assert_nonnull (mcp_stdio_transport_get_input_stream (transport));
    g_assert_nonnull (mcp_stdio_transport_get_output_stream (transport));
}

static void
test_stdio_transport_with_streams (void)
{
    g_autoptr(McpStdioTransport) transport = NULL;
    g_autoptr(GInputStream) input = NULL;
    g_autoptr(GOutputStream) output = NULL;

    /* Create memory streams for testing */
    input = g_memory_input_stream_new ();
    output = g_memory_output_stream_new_resizable ();

    transport = mcp_stdio_transport_new_with_streams (input, output);
    g_assert_nonnull (transport);
    g_assert_true (MCP_IS_STDIO_TRANSPORT (transport));

    g_assert_true (mcp_stdio_transport_get_input_stream (transport) == input);
    g_assert_true (mcp_stdio_transport_get_output_stream (transport) == output);
}

/* Test with memory streams to verify message framing */

typedef struct
{
    McpStdioTransport *transport;
    GMainLoop         *loop;
    GInputStream      *input;
    GOutputStream     *output;
    gboolean           callback_called;
    gboolean           success;
    GError            *error;
    JsonNode          *received_message;
} StdioFixture;

static void
stdio_fixture_setup (StdioFixture  *fixture,
                     gconstpointer  user_data)
{
    fixture->input = g_memory_input_stream_new ();
    fixture->output = g_memory_output_stream_new_resizable ();
    fixture->transport = mcp_stdio_transport_new_with_streams (fixture->input,
                                                               fixture->output);
    fixture->loop = g_main_loop_new (NULL, FALSE);
    fixture->callback_called = FALSE;
    fixture->success = FALSE;
    fixture->error = NULL;
    fixture->received_message = NULL;
}

static void
stdio_fixture_teardown (StdioFixture  *fixture,
                        gconstpointer  user_data)
{
    g_clear_object (&fixture->transport);
    g_clear_object (&fixture->input);
    g_clear_object (&fixture->output);
    g_clear_pointer (&fixture->loop, g_main_loop_unref);
    g_clear_error (&fixture->error);
    g_clear_pointer (&fixture->received_message, json_node_unref);
}

static void
stdio_connect_cb (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    StdioFixture *fixture = user_data;

    fixture->callback_called = TRUE;
    fixture->success = mcp_transport_connect_finish (MCP_TRANSPORT (source),
                                                     result,
                                                     &fixture->error);
    g_main_loop_quit (fixture->loop);
}

static void
test_stdio_transport_connect (StdioFixture  *fixture,
                              gconstpointer  user_data)
{
    mcp_transport_connect_async (MCP_TRANSPORT (fixture->transport),
                                 NULL, stdio_connect_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (fixture->transport)),
                     ==, MCP_TRANSPORT_STATE_CONNECTED);
}

static void
stdio_send_cb (GObject      *source,
               GAsyncResult *result,
               gpointer      user_data)
{
    StdioFixture *fixture = user_data;

    fixture->callback_called = TRUE;
    fixture->success = mcp_transport_send_message_finish (MCP_TRANSPORT (source),
                                                          result,
                                                          &fixture->error);
    g_main_loop_quit (fixture->loop);
}

static void
test_stdio_transport_send (StdioFixture  *fixture,
                           gconstpointer  user_data)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) message = NULL;
    GMemoryOutputStream *mem_output;
    gpointer data;
    gsize size;
    g_autofree gchar *sent_str = NULL;

    /* Connect first */
    mcp_transport_connect_async (MCP_TRANSPORT (fixture->transport),
                                 NULL, stdio_connect_cb, fixture);
    g_main_loop_run (fixture->loop);
    g_assert_true (fixture->success);

    /* Reset fixture state */
    fixture->callback_called = FALSE;
    fixture->success = FALSE;

    /* Build a test message */
    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "jsonrpc");
    json_builder_add_string_value (builder, "2.0");
    json_builder_set_member_name (builder, "method");
    json_builder_add_string_value (builder, "ping");
    json_builder_set_member_name (builder, "id");
    json_builder_add_int_value (builder, 42);
    json_builder_end_object (builder);
    message = json_builder_get_root (builder);

    /* Send the message */
    mcp_transport_send_message_async (MCP_TRANSPORT (fixture->transport),
                                      message, NULL, stdio_send_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);

    /* Verify the output stream has the NDJSON formatted message */
    mem_output = G_MEMORY_OUTPUT_STREAM (fixture->output);
    data = g_memory_output_stream_get_data (mem_output);
    size = g_memory_output_stream_get_data_size (mem_output);

    sent_str = g_strndup (data, size);

    /* Should be JSON followed by newline */
    g_assert_true (g_str_has_suffix (sent_str, "\n"));

    /* Parse it back to verify it's valid JSON */
    {
        g_autoptr(JsonParser) parser = json_parser_new ();
        g_autoptr(GError) parse_error = NULL;
        JsonNode *root;
        JsonObject *obj;

        /* Remove trailing newline for parsing */
        sent_str[size - 1] = '\0';

        g_assert_true (json_parser_load_from_data (parser, sent_str, -1, &parse_error));
        g_assert_no_error (parse_error);

        root = json_parser_get_root (parser);
        obj = json_node_get_object (root);

        g_assert_cmpstr (json_object_get_string_member (obj, "jsonrpc"), ==, "2.0");
        g_assert_cmpstr (json_object_get_string_member (obj, "method"), ==, "ping");
        g_assert_cmpint (json_object_get_int_member (obj, "id"), ==, 42);
    }
}

/* ========================================================================== */
/* Main                                                                       */
/* ========================================================================== */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Transport interface tests using mock */
    g_test_add ("/transport/mock/initial-state",
                TransportFixture, NULL,
                transport_fixture_setup,
                test_transport_initial_state,
                transport_fixture_teardown);

    g_test_add ("/transport/mock/connect",
                TransportFixture, NULL,
                transport_fixture_setup,
                test_transport_connect,
                transport_fixture_teardown);

    g_test_add ("/transport/mock/connect-failure",
                TransportFixture, NULL,
                transport_fixture_setup,
                test_transport_connect_failure,
                transport_fixture_teardown);

    g_test_add ("/transport/mock/disconnect",
                TransportFixture, NULL,
                transport_fixture_setup,
                test_transport_disconnect,
                transport_fixture_teardown);

    g_test_add ("/transport/mock/send-message",
                TransportFixture, NULL,
                transport_fixture_setup,
                test_transport_send_message,
                transport_fixture_teardown);

    g_test_add ("/transport/mock/send-not-connected",
                TransportFixture, NULL,
                transport_fixture_setup,
                test_transport_send_not_connected,
                transport_fixture_teardown);

    g_test_add ("/transport/mock/state-signal",
                TransportFixture, NULL,
                transport_fixture_setup,
                test_transport_state_signal,
                transport_fixture_teardown);

    g_test_add ("/transport/mock/message-signal",
                TransportFixture, NULL,
                transport_fixture_setup,
                test_transport_message_signal,
                transport_fixture_teardown);

    g_test_add ("/transport/mock/error-signal",
                TransportFixture, NULL,
                transport_fixture_setup,
                test_transport_error_signal,
                transport_fixture_teardown);

    /* Stdio transport tests */
    g_test_add_func ("/transport/stdio/new", test_stdio_transport_new);
    g_test_add_func ("/transport/stdio/with-streams", test_stdio_transport_with_streams);

    g_test_add ("/transport/stdio/connect",
                StdioFixture, NULL,
                stdio_fixture_setup,
                test_stdio_transport_connect,
                stdio_fixture_teardown);

    g_test_add ("/transport/stdio/send",
                StdioFixture, NULL,
                stdio_fixture_setup,
                test_stdio_transport_send,
                stdio_fixture_teardown);

    return g_test_run ();
}
