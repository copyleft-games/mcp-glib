/* Regression coverage for pending outbound requests with an unresponsive peer. */
#define MCP_COMPILATION
#include "mcp.h"
#include "mcp-mux-transport.h"

typedef struct
{
    GMainContext *context;
    McpMuxTransport *transport;
    McpClient *client;
    McpServer *server;
    GCancellable *cancellable;
    GError *error;
    guint callbacks;
    gboolean connecting;
} Fixture;

/* Accept frames without replying, so cancellation cannot accidentally depend
 * on peer progress. A missing callback separately exercises send failure. */
static void
discard_frame (McpMuxTransport *transport, JsonNode *frame, gpointer data)
{
    (void) transport;
    (void) frame;
    (void) data;
}

static void
set_up (Fixture *fixture, gconstpointer data)
{
    (void) data;
    fixture->context = g_main_context_new ();
    g_main_context_push_thread_default (fixture->context);
    fixture->transport = mcp_mux_transport_new (fixture->context);
    mcp_mux_transport_set_send_callback (fixture->transport, discard_frame, NULL, NULL);
    mcp_mux_transport_set_connected (fixture->transport, TRUE);
    fixture->client = mcp_client_new ("test-client", "1");
    mcp_client_set_transport (fixture->client, MCP_TRANSPORT (fixture->transport));
    fixture->server = mcp_server_new ("test-server", "1");
    mcp_server_set_transport (fixture->server, MCP_TRANSPORT (fixture->transport));
    mcp_session_set_state (MCP_SESSION (fixture->client), MCP_SESSION_STATE_READY);
    mcp_session_set_state (MCP_SESSION (fixture->server), MCP_SESSION_STATE_READY);
    fixture->cancellable = g_cancellable_new ();
}

static void
tear_down (Fixture *fixture, gconstpointer data)
{
    (void) data;
    mcp_server_stop (fixture->server);
    while (g_main_context_iteration (fixture->context, FALSE)) {}
    g_clear_object (&fixture->client);
    g_clear_object (&fixture->server);
    g_clear_object (&fixture->transport);
    g_clear_object (&fixture->cancellable);
    g_clear_error (&fixture->error);
    g_main_context_pop_thread_default (fixture->context);
    g_main_context_unref (fixture->context);
}

/* Count callbacks so a late reply or disconnect cannot silently complete twice. */
static void
on_result (GObject *source, GAsyncResult *result, gpointer data)
{
    Fixture *fixture = data;
    if (fixture->connecting)
        mcp_client_connect_finish (MCP_CLIENT (source), result, &fixture->error);
    else if (MCP_IS_CLIENT (source))
    {
        GList *tools = mcp_client_list_tools_finish (MCP_CLIENT (source), result, &fixture->error);
        g_list_free_full (tools, g_object_unref);
    }
    else
    {
        GList *roots = mcp_server_list_roots_finish (MCP_SERVER (source), result, &fixture->error);
        g_list_free_full (roots, g_object_unref);
    }
    fixture->callbacks++;
}

static void
wait_callback (Fixture *fixture)
{
    gint64 deadline = g_get_monotonic_time () + G_USEC_PER_SEC;
    while (fixture->callbacks == 0 && g_get_monotonic_time () < deadline)
        g_main_context_iteration (fixture->context, FALSE);
    g_assert_cmpuint (fixture->callbacks, ==, 1);
}

static void
test_cancel (Fixture *fixture, gconstpointer data)
{
    gboolean client = GPOINTER_TO_INT (data);
    McpSession *session = client ? MCP_SESSION (fixture->client) : MCP_SESSION (fixture->server);
    if (client)
        mcp_client_list_tools_async (fixture->client, fixture->cancellable, on_result, fixture);
    else
        mcp_server_list_roots_async (fixture->server, fixture->cancellable, on_result, fixture);
    g_assert_cmpuint (mcp_session_get_pending_request_count (session), ==, 1);
    g_cancellable_cancel (fixture->cancellable);
    wait_callback (fixture);
    g_assert_error (fixture->error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_assert_cmpuint (mcp_session_get_pending_request_count (session), ==, 0);
    /* A peer may race cancellation and reply afterward. It must neither
     * complete twice nor produce a fatal warning in strict applications. */
    {
        g_autoptr(JsonParser) parser = json_parser_new ();
        g_assert_true (json_parser_load_from_data (parser,
            "{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":{}}", -1, NULL));
        mcp_mux_transport_dispatch_frame (fixture->transport, json_parser_get_root (parser));
        while (g_main_context_iteration (fixture->context, FALSE)) {}
        g_assert_cmpuint (fixture->callbacks, ==, 1);
    }
}

static void
test_send_error (Fixture *fixture, gconstpointer data)
{
    gboolean client = GPOINTER_TO_INT (data);
    mcp_mux_transport_set_send_callback (fixture->transport, NULL, NULL, NULL);
    if (client)
        mcp_client_list_tools_async (fixture->client, NULL, on_result, fixture);
    else
        mcp_server_list_roots_async (fixture->server, NULL, on_result, fixture);
    wait_callback (fixture);
    g_assert_nonnull (fixture->error);
    g_assert_cmpuint (mcp_session_get_pending_request_count (client ? MCP_SESSION (fixture->client) : MCP_SESSION (fixture->server)), ==, 0);
}

static void
test_initialize_cancel (Fixture *fixture, gconstpointer data)
{
    gint64 deadline;
    (void) data;
    fixture->connecting = TRUE;
    mcp_client_connect_async (fixture->client, fixture->cancellable, on_result, fixture);
    deadline = g_get_monotonic_time () + G_USEC_PER_SEC;
    while (mcp_session_get_pending_request_count (MCP_SESSION (fixture->client)) == 0 &&
           g_get_monotonic_time () < deadline)
        g_main_context_iteration (fixture->context, FALSE);
    g_assert_cmpuint (mcp_session_get_pending_request_count (MCP_SESSION (fixture->client)), ==, 1);
    g_cancellable_cancel (fixture->cancellable);
    wait_callback (fixture);
    g_assert_error (fixture->error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_assert_cmpuint (mcp_session_get_pending_request_count (MCP_SESSION (fixture->client)), ==, 0);
    /* A second connect must not be rejected by a retained connect_task. It
     * reaches the carrier and fails because this deliberately removes send. */
    fixture->callbacks = 0;
    g_clear_error (&fixture->error);
    mcp_mux_transport_set_send_callback (fixture->transport, NULL, NULL, NULL);
    mcp_client_connect_async (fixture->client, NULL, on_result, fixture);
    wait_callback (fixture);
    g_assert_error (fixture->error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    mcp_mux_transport_set_connected (fixture->transport, FALSE);
    while (g_main_context_iteration (fixture->context, FALSE)) {}
    g_assert_cmpuint (fixture->callbacks, ==, 1);
}

/* Both peer rejection and clean closure retire initialize's dual ownership. */
static void
test_initialize_failure (Fixture *fixture, gconstpointer data)
{
    gint64 deadline;
    fixture->connecting = TRUE;
    mcp_client_connect_async (fixture->client, NULL, on_result, fixture);
    deadline = g_get_monotonic_time () + G_USEC_PER_SEC;
    while (mcp_session_get_pending_request_count (MCP_SESSION (fixture->client)) == 0 &&
           g_get_monotonic_time () < deadline)
        g_main_context_iteration (fixture->context, FALSE);
    g_assert_cmpuint (mcp_session_get_pending_request_count (MCP_SESSION (fixture->client)), ==, 1);
    if (GPOINTER_TO_INT (data))
    {
        g_autoptr(JsonParser) parser = json_parser_new ();
        g_assert_true (json_parser_load_from_data (parser,
            "{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"error\":{\"code\":-32602,\"message\":\"rejected\"}}", -1, NULL));
        mcp_mux_transport_dispatch_frame (fixture->transport, json_parser_get_root (parser));
    }
    else
        mcp_mux_transport_set_connected (fixture->transport, FALSE);
    wait_callback (fixture);
    g_assert_nonnull (fixture->error);
    g_assert_cmpuint (mcp_session_get_pending_request_count (MCP_SESSION (fixture->client)), ==, 0);
    fixture->callbacks = 0;
    g_clear_error (&fixture->error);
    mcp_mux_transport_set_connected (fixture->transport, TRUE);
    mcp_mux_transport_set_send_callback (fixture->transport, NULL, NULL, NULL);
    mcp_client_connect_async (fixture->client, NULL, on_result, fixture);
    wait_callback (fixture);
    g_assert_error (fixture->error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
}

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_add ("/requests/client-cancel", Fixture, GINT_TO_POINTER (1), set_up, test_cancel, tear_down);
    g_test_add ("/requests/server-cancel", Fixture, NULL, set_up, test_cancel, tear_down);
    g_test_add ("/requests/client-send-error", Fixture, GINT_TO_POINTER (1), set_up, test_send_error, tear_down);
    g_test_add ("/requests/server-send-error", Fixture, NULL, set_up, test_send_error, tear_down);
    g_test_add ("/requests/initialize-cancel-reconnect", Fixture, NULL, set_up, test_initialize_cancel, tear_down);
    g_test_add ("/requests/initialize-disconnect", Fixture, NULL, set_up, test_initialize_failure, tear_down);
    g_test_add ("/requests/initialize-rejected", Fixture, GINT_TO_POINTER (1), set_up, test_initialize_failure, tear_down);
    return g_test_run ();
}
