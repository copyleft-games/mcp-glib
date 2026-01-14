/*
 * test-http-server-transport.c - Tests for McpHttpServerTransport
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include "mcp.h"

/* ============================================================================
 * Constructor Tests
 * ========================================================================== */

/* Test basic HTTP server transport creation */
static void
test_http_server_transport_new (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);
    g_assert_nonnull (transport);
    g_assert_true (MCP_IS_HTTP_SERVER_TRANSPORT (transport));
    g_assert_true (MCP_IS_TRANSPORT (transport));
}

/* Test HTTP server transport creation with port 0 (auto-assign) */
static void
test_http_server_transport_new_port_zero (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (0);
    g_assert_nonnull (transport);
    g_assert_cmpuint (mcp_http_server_transport_get_port (transport), ==, 0);
}

/* Test HTTP server transport creation with full parameters */
static void
test_http_server_transport_new_full (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new_full ("127.0.0.1", 9000);
    g_assert_nonnull (transport);
    g_assert_cmpstr (mcp_http_server_transport_get_host (transport), ==, "127.0.0.1");
    g_assert_cmpuint (mcp_http_server_transport_get_port (transport), ==, 9000);
}

/* ============================================================================
 * Property Tests
 * ========================================================================== */

/* Test port property */
static void
test_http_server_transport_port (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);

    g_assert_cmpuint (mcp_http_server_transport_get_port (transport), ==, 8080);

    mcp_http_server_transport_set_port (transport, 9000);
    g_assert_cmpuint (mcp_http_server_transport_get_port (transport), ==, 9000);
}

/* Test host property */
static void
test_http_server_transport_host (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);

    /* Default is NULL (all interfaces) */
    g_assert_null (mcp_http_server_transport_get_host (transport));

    mcp_http_server_transport_set_host (transport, "127.0.0.1");
    g_assert_cmpstr (mcp_http_server_transport_get_host (transport), ==, "127.0.0.1");

    mcp_http_server_transport_set_host (transport, NULL);
    g_assert_null (mcp_http_server_transport_get_host (transport));
}

/* Test POST path property */
static void
test_http_server_transport_post_path (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);

    /* Default is "/" */
    g_assert_cmpstr (mcp_http_server_transport_get_post_path (transport), ==, "/");

    mcp_http_server_transport_set_post_path (transport, "/mcp");
    g_assert_cmpstr (mcp_http_server_transport_get_post_path (transport), ==, "/mcp");
}

/* Test SSE path property */
static void
test_http_server_transport_sse_path (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);

    /* Default is "/sse" */
    g_assert_cmpstr (mcp_http_server_transport_get_sse_path (transport), ==, "/sse");

    mcp_http_server_transport_set_sse_path (transport, "/events");
    g_assert_cmpstr (mcp_http_server_transport_get_sse_path (transport), ==, "/events");
}

/* Test session ID (read-only) */
static void
test_http_server_transport_session_id (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);

    /* Session ID is NULL before client connects */
    g_assert_null (mcp_http_server_transport_get_session_id (transport));
}

/* Test require-auth property */
static void
test_http_server_transport_require_auth (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);

    /* Default is FALSE */
    g_assert_false (mcp_http_server_transport_get_require_auth (transport));

    mcp_http_server_transport_set_require_auth (transport, TRUE);
    g_assert_true (mcp_http_server_transport_get_require_auth (transport));

    mcp_http_server_transport_set_require_auth (transport, FALSE);
    g_assert_false (mcp_http_server_transport_get_require_auth (transport));
}

/* Test auth-token property */
static void
test_http_server_transport_auth_token (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);

    /* Default is NULL */
    g_assert_null (mcp_http_server_transport_get_auth_token (transport));

    mcp_http_server_transport_set_auth_token (transport, "secret-token-123");
    g_assert_cmpstr (mcp_http_server_transport_get_auth_token (transport), ==, "secret-token-123");

    mcp_http_server_transport_set_auth_token (transport, NULL);
    g_assert_null (mcp_http_server_transport_get_auth_token (transport));
}

/* Test TLS certificate property */
static void
test_http_server_transport_tls_certificate (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);

    /* Default is NULL */
    g_assert_null (mcp_http_server_transport_get_tls_certificate (transport));
}

/* Test has_client property */
static void
test_http_server_transport_has_client (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);

    /* No client connected initially */
    g_assert_false (mcp_http_server_transport_has_client (transport));
}

/* Test soup server accessor (NULL before connect) */
static void
test_http_server_transport_soup_server (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);

    /* Soup server is NULL before connect */
    g_assert_null (mcp_http_server_transport_get_soup_server (transport));
}

/* Test actual port (0 before connect) */
static void
test_http_server_transport_actual_port (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (0);

    /* Actual port is 0 before connect */
    g_assert_cmpuint (mcp_http_server_transport_get_actual_port (transport), ==, 0);
}

/* ============================================================================
 * Interface Tests
 * ========================================================================== */

/* Test initial transport state */
static void
test_http_server_transport_initial_state (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (8080);

    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (transport)), ==,
                     MCP_TRANSPORT_STATE_DISCONNECTED);
    g_assert_false (mcp_transport_is_connected (MCP_TRANSPORT (transport)));
}

/* Test that transport implements McpTransport interface */
static void
test_http_server_transport_interface (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;
    McpTransportInterface *iface;

    transport = mcp_http_server_transport_new (8080);
    iface = MCP_TRANSPORT_GET_IFACE (transport);

    g_assert_nonnull (iface->get_state);
    g_assert_nonnull (iface->connect_async);
    g_assert_nonnull (iface->connect_finish);
    g_assert_nonnull (iface->disconnect_async);
    g_assert_nonnull (iface->disconnect_finish);
    g_assert_nonnull (iface->send_message_async);
    g_assert_nonnull (iface->send_message_finish);
}

/* ============================================================================
 * GObject Property Tests
 * ========================================================================== */

/* Test properties via GObject get/set */
static void
test_http_server_transport_gobject_properties (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;
    g_autofree gchar *host = NULL;
    g_autofree gchar *post_path = NULL;
    g_autofree gchar *sse_path = NULL;
    g_autofree gchar *auth_token = NULL;
    guint port;
    gboolean require_auth;

    transport = mcp_http_server_transport_new (8080);

    /* Set some properties */
    mcp_http_server_transport_set_host (transport, "localhost");
    mcp_http_server_transport_set_post_path (transport, "/api");
    mcp_http_server_transport_set_sse_path (transport, "/stream");
    mcp_http_server_transport_set_require_auth (transport, TRUE);
    mcp_http_server_transport_set_auth_token (transport, "token123");

    /* Get via GObject */
    g_object_get (transport,
                  "host", &host,
                  "port", &port,
                  "post-path", &post_path,
                  "sse-path", &sse_path,
                  "require-auth", &require_auth,
                  "auth-token", &auth_token,
                  NULL);

    g_assert_cmpstr (host, ==, "localhost");
    g_assert_cmpuint (port, ==, 8080);
    g_assert_cmpstr (post_path, ==, "/api");
    g_assert_cmpstr (sse_path, ==, "/stream");
    g_assert_true (require_auth);
    g_assert_cmpstr (auth_token, ==, "token123");
}

/* Test setting properties via GObject */
static void
test_http_server_transport_gobject_set_properties (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;

    transport = mcp_http_server_transport_new (0);

    g_object_set (transport,
                  "host", "0.0.0.0",
                  "port", 9999,
                  "post-path", "/rpc",
                  "sse-path", "/events",
                  "require-auth", TRUE,
                  "auth-token", "mysecret",
                  NULL);

    g_assert_cmpstr (mcp_http_server_transport_get_host (transport), ==, "0.0.0.0");
    g_assert_cmpuint (mcp_http_server_transport_get_port (transport), ==, 9999);
    g_assert_cmpstr (mcp_http_server_transport_get_post_path (transport), ==, "/rpc");
    g_assert_cmpstr (mcp_http_server_transport_get_sse_path (transport), ==, "/events");
    g_assert_true (mcp_http_server_transport_get_require_auth (transport));
    g_assert_cmpstr (mcp_http_server_transport_get_auth_token (transport), ==, "mysecret");
}

/* ============================================================================
 * Server Connect/Disconnect Tests
 * ========================================================================== */

/* Helper for async tests */
typedef struct
{
    GMainLoop *loop;
    gboolean   success;
    GError    *error;
} AsyncTestData;

static void
on_connect_finished (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
    AsyncTestData *data = user_data;
    data->success = mcp_transport_connect_finish (MCP_TRANSPORT (source), result, &data->error);
    g_main_loop_quit (data->loop);
}

static void
on_disconnect_finished (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
    AsyncTestData *data = user_data;
    data->success = mcp_transport_disconnect_finish (MCP_TRANSPORT (source), result, &data->error);
    g_main_loop_quit (data->loop);
}

/* Test connecting server transport */
static void
test_http_server_transport_connect (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };

    transport = mcp_http_server_transport_new (0);  /* Port 0 = auto-assign */
    loop = g_main_loop_new (NULL, FALSE);
    data.loop = loop;

    mcp_transport_connect_async (MCP_TRANSPORT (transport), NULL, on_connect_finished, &data);
    g_main_loop_run (loop);

    g_assert_true (data.success);
    g_assert_no_error (data.error);
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (transport)), ==,
                     MCP_TRANSPORT_STATE_CONNECTED);
    g_assert_true (mcp_transport_is_connected (MCP_TRANSPORT (transport)));

    /* Check that actual port was assigned */
    g_assert_cmpuint (mcp_http_server_transport_get_actual_port (transport), >, 0);

    /* Check that soup server is available */
    g_assert_nonnull (mcp_http_server_transport_get_soup_server (transport));
    g_assert_true (SOUP_IS_SERVER (mcp_http_server_transport_get_soup_server (transport)));

    g_clear_error (&data.error);
}

/* Test disconnecting server transport */
static void
test_http_server_transport_disconnect (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };

    transport = mcp_http_server_transport_new (0);
    loop = g_main_loop_new (NULL, FALSE);
    data.loop = loop;

    /* First connect */
    mcp_transport_connect_async (MCP_TRANSPORT (transport), NULL, on_connect_finished, &data);
    g_main_loop_run (loop);
    g_assert_true (data.success);
    g_clear_error (&data.error);

    /* Then disconnect */
    data.success = FALSE;
    mcp_transport_disconnect_async (MCP_TRANSPORT (transport), NULL, on_disconnect_finished, &data);
    g_main_loop_run (loop);

    g_assert_true (data.success);
    g_assert_no_error (data.error);
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (transport)), ==,
                     MCP_TRANSPORT_STATE_DISCONNECTED);
    g_assert_false (mcp_transport_is_connected (MCP_TRANSPORT (transport)));

    g_clear_error (&data.error);
}

/* Test double connect (should fail) */
static void
test_http_server_transport_double_connect (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };

    transport = mcp_http_server_transport_new (0);
    loop = g_main_loop_new (NULL, FALSE);
    data.loop = loop;

    /* First connect */
    mcp_transport_connect_async (MCP_TRANSPORT (transport), NULL, on_connect_finished, &data);
    g_main_loop_run (loop);
    g_assert_true (data.success);
    g_clear_error (&data.error);

    /* Second connect should fail */
    data.success = FALSE;
    mcp_transport_connect_async (MCP_TRANSPORT (transport), NULL, on_connect_finished, &data);
    g_main_loop_run (loop);

    g_assert_false (data.success);
    g_assert_error (data.error, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR);

    g_clear_error (&data.error);
}

/* Test specific port binding */
static void
test_http_server_transport_specific_port (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };
    guint port;

    /* Use a high port to avoid conflicts - use ephemeral port range */
    transport = mcp_http_server_transport_new (0);  /* Let OS assign */
    loop = g_main_loop_new (NULL, FALSE);
    data.loop = loop;

    mcp_transport_connect_async (MCP_TRANSPORT (transport), NULL, on_connect_finished, &data);
    g_main_loop_run (loop);

    g_assert_true (data.success);
    port = mcp_http_server_transport_get_actual_port (transport);
    g_assert_cmpuint (port, >, 0);

    g_clear_error (&data.error);
}

/* Test localhost binding */
static void
test_http_server_transport_localhost_binding (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };

    transport = mcp_http_server_transport_new_full ("127.0.0.1", 0);
    loop = g_main_loop_new (NULL, FALSE);
    data.loop = loop;

    mcp_transport_connect_async (MCP_TRANSPORT (transport), NULL, on_connect_finished, &data);
    g_main_loop_run (loop);

    g_assert_true (data.success);
    g_assert_no_error (data.error);
    g_assert_true (mcp_transport_is_connected (MCP_TRANSPORT (transport)));

    g_clear_error (&data.error);
}

/* ============================================================================
 * Signal Tests
 * ========================================================================== */

static void
on_state_changed (McpTransport      *transport,
                  McpTransportState  old_state,
                  McpTransportState  new_state,
                  gpointer           user_data)
{
    gint *count = user_data;
    (*count)++;
}

/* Test state-changed signal on connect */
static void
test_http_server_transport_state_changed_signal (void)
{
    g_autoptr(McpHttpServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };
    gint state_change_count = 0;

    transport = mcp_http_server_transport_new (0);
    loop = g_main_loop_new (NULL, FALSE);
    data.loop = loop;

    g_signal_connect (transport, "state-changed",
                      G_CALLBACK (on_state_changed), &state_change_count);

    mcp_transport_connect_async (MCP_TRANSPORT (transport), NULL, on_connect_finished, &data);
    g_main_loop_run (loop);

    g_assert_true (data.success);
    /* Should have at least one state change (DISCONNECTED -> CONNECTING -> CONNECTED) */
    g_assert_cmpint (state_change_count, >=, 1);

    g_clear_error (&data.error);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Constructor tests */
    g_test_add_func ("/mcp/http-server-transport/constructor/new",
                     test_http_server_transport_new);
    g_test_add_func ("/mcp/http-server-transport/constructor/new-port-zero",
                     test_http_server_transport_new_port_zero);
    g_test_add_func ("/mcp/http-server-transport/constructor/new-full",
                     test_http_server_transport_new_full);

    /* Property tests */
    g_test_add_func ("/mcp/http-server-transport/properties/port",
                     test_http_server_transport_port);
    g_test_add_func ("/mcp/http-server-transport/properties/host",
                     test_http_server_transport_host);
    g_test_add_func ("/mcp/http-server-transport/properties/post-path",
                     test_http_server_transport_post_path);
    g_test_add_func ("/mcp/http-server-transport/properties/sse-path",
                     test_http_server_transport_sse_path);
    g_test_add_func ("/mcp/http-server-transport/properties/session-id",
                     test_http_server_transport_session_id);
    g_test_add_func ("/mcp/http-server-transport/properties/require-auth",
                     test_http_server_transport_require_auth);
    g_test_add_func ("/mcp/http-server-transport/properties/auth-token",
                     test_http_server_transport_auth_token);
    g_test_add_func ("/mcp/http-server-transport/properties/tls-certificate",
                     test_http_server_transport_tls_certificate);
    g_test_add_func ("/mcp/http-server-transport/properties/has-client",
                     test_http_server_transport_has_client);
    g_test_add_func ("/mcp/http-server-transport/properties/soup-server",
                     test_http_server_transport_soup_server);
    g_test_add_func ("/mcp/http-server-transport/properties/actual-port",
                     test_http_server_transport_actual_port);

    /* Interface tests */
    g_test_add_func ("/mcp/http-server-transport/interface/initial-state",
                     test_http_server_transport_initial_state);
    g_test_add_func ("/mcp/http-server-transport/interface/implements-transport",
                     test_http_server_transport_interface);

    /* GObject property tests */
    g_test_add_func ("/mcp/http-server-transport/gobject/get-properties",
                     test_http_server_transport_gobject_properties);
    g_test_add_func ("/mcp/http-server-transport/gobject/set-properties",
                     test_http_server_transport_gobject_set_properties);

    /* Connect/Disconnect tests */
    g_test_add_func ("/mcp/http-server-transport/connect/basic",
                     test_http_server_transport_connect);
    g_test_add_func ("/mcp/http-server-transport/connect/disconnect",
                     test_http_server_transport_disconnect);
    g_test_add_func ("/mcp/http-server-transport/connect/double-connect",
                     test_http_server_transport_double_connect);
    g_test_add_func ("/mcp/http-server-transport/connect/specific-port",
                     test_http_server_transport_specific_port);
    g_test_add_func ("/mcp/http-server-transport/connect/localhost-binding",
                     test_http_server_transport_localhost_binding);

    /* Signal tests */
    g_test_add_func ("/mcp/http-server-transport/signals/state-changed",
                     test_http_server_transport_state_changed_signal);

    return g_test_run ();
}
