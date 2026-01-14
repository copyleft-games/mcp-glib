/*
 * test-websocket-server-transport.c - Tests for McpWebSocketServerTransport
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include "mcp.h"

/* ============================================================================
 * Constructor Tests
 * ========================================================================== */

/* Test basic WebSocket server transport creation */
static void
test_websocket_server_transport_new (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);
    g_assert_nonnull (transport);
    g_assert_true (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (transport));
    g_assert_true (MCP_IS_TRANSPORT (transport));
}

/* Test WebSocket server transport creation with port 0 (auto-assign) */
static void
test_websocket_server_transport_new_port_zero (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (0);
    g_assert_nonnull (transport);
    g_assert_cmpuint (mcp_websocket_server_transport_get_port (transport), ==, 0);
}

/* Test WebSocket server transport creation with full parameters */
static void
test_websocket_server_transport_new_full (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new_full ("127.0.0.1", 9000, "/ws");
    g_assert_nonnull (transport);
    g_assert_cmpstr (mcp_websocket_server_transport_get_host (transport), ==, "127.0.0.1");
    g_assert_cmpuint (mcp_websocket_server_transport_get_port (transport), ==, 9000);
    g_assert_cmpstr (mcp_websocket_server_transport_get_path (transport), ==, "/ws");
}

/* ============================================================================
 * Property Tests
 * ========================================================================== */

/* Test port property */
static void
test_websocket_server_transport_port (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    g_assert_cmpuint (mcp_websocket_server_transport_get_port (transport), ==, 8080);

    mcp_websocket_server_transport_set_port (transport, 9000);
    g_assert_cmpuint (mcp_websocket_server_transport_get_port (transport), ==, 9000);
}

/* Test host property */
static void
test_websocket_server_transport_host (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    /* Default is NULL (all interfaces) */
    g_assert_null (mcp_websocket_server_transport_get_host (transport));

    mcp_websocket_server_transport_set_host (transport, "127.0.0.1");
    g_assert_cmpstr (mcp_websocket_server_transport_get_host (transport), ==, "127.0.0.1");

    mcp_websocket_server_transport_set_host (transport, NULL);
    g_assert_null (mcp_websocket_server_transport_get_host (transport));
}

/* Test path property */
static void
test_websocket_server_transport_path (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    /* Default is "/" */
    g_assert_cmpstr (mcp_websocket_server_transport_get_path (transport), ==, "/");

    mcp_websocket_server_transport_set_path (transport, "/mcp");
    g_assert_cmpstr (mcp_websocket_server_transport_get_path (transport), ==, "/mcp");
}

/* Test protocols property */
static void
test_websocket_server_transport_protocols (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;
    const gchar * const *protocols;
    const gchar * const new_protocols[] = { "mcp", "json-rpc", NULL };

    transport = mcp_websocket_server_transport_new (8080);

    /* Default is NULL */
    protocols = mcp_websocket_server_transport_get_protocols (transport);
    g_assert_null (protocols);

    mcp_websocket_server_transport_set_protocols (transport, new_protocols);
    protocols = mcp_websocket_server_transport_get_protocols (transport);
    g_assert_nonnull (protocols);
    g_assert_cmpstr (protocols[0], ==, "mcp");
    g_assert_cmpstr (protocols[1], ==, "json-rpc");
    g_assert_null (protocols[2]);
}

/* Test origin property */
static void
test_websocket_server_transport_origin (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    /* Default is NULL (accept any origin) */
    g_assert_null (mcp_websocket_server_transport_get_origin (transport));

    mcp_websocket_server_transport_set_origin (transport, "http://localhost:3000");
    g_assert_cmpstr (mcp_websocket_server_transport_get_origin (transport), ==,
                     "http://localhost:3000");

    mcp_websocket_server_transport_set_origin (transport, NULL);
    g_assert_null (mcp_websocket_server_transport_get_origin (transport));
}

/* Test require-auth property */
static void
test_websocket_server_transport_require_auth (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    /* Default is FALSE */
    g_assert_false (mcp_websocket_server_transport_get_require_auth (transport));

    mcp_websocket_server_transport_set_require_auth (transport, TRUE);
    g_assert_true (mcp_websocket_server_transport_get_require_auth (transport));

    mcp_websocket_server_transport_set_require_auth (transport, FALSE);
    g_assert_false (mcp_websocket_server_transport_get_require_auth (transport));
}

/* Test auth-token property */
static void
test_websocket_server_transport_auth_token (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    /* Default is NULL */
    g_assert_null (mcp_websocket_server_transport_get_auth_token (transport));

    mcp_websocket_server_transport_set_auth_token (transport, "secret-token-123");
    g_assert_cmpstr (mcp_websocket_server_transport_get_auth_token (transport), ==,
                     "secret-token-123");

    mcp_websocket_server_transport_set_auth_token (transport, NULL);
    g_assert_null (mcp_websocket_server_transport_get_auth_token (transport));
}

/* Test keepalive-interval property */
static void
test_websocket_server_transport_keepalive_interval (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    /* Default is 30 seconds */
    g_assert_cmpuint (mcp_websocket_server_transport_get_keepalive_interval (transport), ==, 30);

    mcp_websocket_server_transport_set_keepalive_interval (transport, 60);
    g_assert_cmpuint (mcp_websocket_server_transport_get_keepalive_interval (transport), ==, 60);

    /* 0 disables keepalive */
    mcp_websocket_server_transport_set_keepalive_interval (transport, 0);
    g_assert_cmpuint (mcp_websocket_server_transport_get_keepalive_interval (transport), ==, 0);
}

/* Test TLS certificate property */
static void
test_websocket_server_transport_tls_certificate (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    /* Default is NULL */
    g_assert_null (mcp_websocket_server_transport_get_tls_certificate (transport));
}

/* Test has_client property */
static void
test_websocket_server_transport_has_client (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    /* No client connected initially */
    g_assert_false (mcp_websocket_server_transport_has_client (transport));
}

/* Test soup server accessor (NULL before connect) */
static void
test_websocket_server_transport_soup_server (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    /* Soup server is NULL before connect */
    g_assert_null (mcp_websocket_server_transport_get_soup_server (transport));
}

/* Test websocket connection accessor (NULL before client connects) */
static void
test_websocket_server_transport_websocket_connection (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    /* WebSocket connection is NULL before client connects */
    g_assert_null (mcp_websocket_server_transport_get_websocket_connection (transport));
}

/* Test actual port (0 before connect) */
static void
test_websocket_server_transport_actual_port (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (0);

    /* Actual port is 0 before connect */
    g_assert_cmpuint (mcp_websocket_server_transport_get_actual_port (transport), ==, 0);
}

/* ============================================================================
 * Interface Tests
 * ========================================================================== */

/* Test initial transport state */
static void
test_websocket_server_transport_initial_state (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (8080);

    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (transport)), ==,
                     MCP_TRANSPORT_STATE_DISCONNECTED);
    g_assert_false (mcp_transport_is_connected (MCP_TRANSPORT (transport)));
}

/* Test that transport implements McpTransport interface */
static void
test_websocket_server_transport_interface (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;
    McpTransportInterface *iface;

    transport = mcp_websocket_server_transport_new (8080);
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
test_websocket_server_transport_gobject_properties (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;
    g_autofree gchar *host = NULL;
    g_autofree gchar *path = NULL;
    g_autofree gchar *origin = NULL;
    g_autofree gchar *auth_token = NULL;
    guint port;
    guint keepalive_interval;
    gboolean require_auth;

    transport = mcp_websocket_server_transport_new (8080);

    /* Set some properties */
    mcp_websocket_server_transport_set_host (transport, "localhost");
    mcp_websocket_server_transport_set_path (transport, "/socket");
    mcp_websocket_server_transport_set_origin (transport, "http://example.com");
    mcp_websocket_server_transport_set_require_auth (transport, TRUE);
    mcp_websocket_server_transport_set_auth_token (transport, "token123");
    mcp_websocket_server_transport_set_keepalive_interval (transport, 45);

    /* Get via GObject */
    g_object_get (transport,
                  "host", &host,
                  "port", &port,
                  "path", &path,
                  "origin", &origin,
                  "require-auth", &require_auth,
                  "auth-token", &auth_token,
                  "keepalive-interval", &keepalive_interval,
                  NULL);

    g_assert_cmpstr (host, ==, "localhost");
    g_assert_cmpuint (port, ==, 8080);
    g_assert_cmpstr (path, ==, "/socket");
    g_assert_cmpstr (origin, ==, "http://example.com");
    g_assert_true (require_auth);
    g_assert_cmpstr (auth_token, ==, "token123");
    g_assert_cmpuint (keepalive_interval, ==, 45);
}

/* Test setting properties via GObject */
static void
test_websocket_server_transport_gobject_set_properties (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;

    transport = mcp_websocket_server_transport_new (0);

    g_object_set (transport,
                  "host", "0.0.0.0",
                  "port", 9999,
                  "path", "/mcp",
                  "origin", "http://localhost",
                  "require-auth", TRUE,
                  "auth-token", "mysecret",
                  "keepalive-interval", 15,
                  NULL);

    g_assert_cmpstr (mcp_websocket_server_transport_get_host (transport), ==, "0.0.0.0");
    g_assert_cmpuint (mcp_websocket_server_transport_get_port (transport), ==, 9999);
    g_assert_cmpstr (mcp_websocket_server_transport_get_path (transport), ==, "/mcp");
    g_assert_cmpstr (mcp_websocket_server_transport_get_origin (transport), ==, "http://localhost");
    g_assert_true (mcp_websocket_server_transport_get_require_auth (transport));
    g_assert_cmpstr (mcp_websocket_server_transport_get_auth_token (transport), ==, "mysecret");
    g_assert_cmpuint (mcp_websocket_server_transport_get_keepalive_interval (transport), ==, 15);
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
test_websocket_server_transport_connect (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };

    transport = mcp_websocket_server_transport_new (0);  /* Port 0 = auto-assign */
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
    g_assert_cmpuint (mcp_websocket_server_transport_get_actual_port (transport), >, 0);

    /* Check that soup server is available */
    g_assert_nonnull (mcp_websocket_server_transport_get_soup_server (transport));
    g_assert_true (SOUP_IS_SERVER (mcp_websocket_server_transport_get_soup_server (transport)));

    g_clear_error (&data.error);
}

/* Test disconnecting server transport */
static void
test_websocket_server_transport_disconnect (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };

    transport = mcp_websocket_server_transport_new (0);
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
test_websocket_server_transport_double_connect (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };

    transport = mcp_websocket_server_transport_new (0);
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

/* Test localhost binding */
static void
test_websocket_server_transport_localhost_binding (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };

    transport = mcp_websocket_server_transport_new_full ("127.0.0.1", 0, "/");
    loop = g_main_loop_new (NULL, FALSE);
    data.loop = loop;

    mcp_transport_connect_async (MCP_TRANSPORT (transport), NULL, on_connect_finished, &data);
    g_main_loop_run (loop);

    g_assert_true (data.success);
    g_assert_no_error (data.error);
    g_assert_true (mcp_transport_is_connected (MCP_TRANSPORT (transport)));

    g_clear_error (&data.error);
}

/* Test custom path */
static void
test_websocket_server_transport_custom_path (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };

    transport = mcp_websocket_server_transport_new_full (NULL, 0, "/api/ws");
    loop = g_main_loop_new (NULL, FALSE);
    data.loop = loop;

    mcp_transport_connect_async (MCP_TRANSPORT (transport), NULL, on_connect_finished, &data);
    g_main_loop_run (loop);

    g_assert_true (data.success);
    g_assert_no_error (data.error);

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
test_websocket_server_transport_state_changed_signal (void)
{
    g_autoptr(McpWebSocketServerTransport) transport = NULL;
    g_autoptr(GMainLoop) loop = NULL;
    AsyncTestData data = { 0 };
    gint state_change_count = 0;

    transport = mcp_websocket_server_transport_new (0);
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
    g_test_add_func ("/mcp/websocket-server-transport/constructor/new",
                     test_websocket_server_transport_new);
    g_test_add_func ("/mcp/websocket-server-transport/constructor/new-port-zero",
                     test_websocket_server_transport_new_port_zero);
    g_test_add_func ("/mcp/websocket-server-transport/constructor/new-full",
                     test_websocket_server_transport_new_full);

    /* Property tests */
    g_test_add_func ("/mcp/websocket-server-transport/properties/port",
                     test_websocket_server_transport_port);
    g_test_add_func ("/mcp/websocket-server-transport/properties/host",
                     test_websocket_server_transport_host);
    g_test_add_func ("/mcp/websocket-server-transport/properties/path",
                     test_websocket_server_transport_path);
    g_test_add_func ("/mcp/websocket-server-transport/properties/protocols",
                     test_websocket_server_transport_protocols);
    g_test_add_func ("/mcp/websocket-server-transport/properties/origin",
                     test_websocket_server_transport_origin);
    g_test_add_func ("/mcp/websocket-server-transport/properties/require-auth",
                     test_websocket_server_transport_require_auth);
    g_test_add_func ("/mcp/websocket-server-transport/properties/auth-token",
                     test_websocket_server_transport_auth_token);
    g_test_add_func ("/mcp/websocket-server-transport/properties/keepalive-interval",
                     test_websocket_server_transport_keepalive_interval);
    g_test_add_func ("/mcp/websocket-server-transport/properties/tls-certificate",
                     test_websocket_server_transport_tls_certificate);
    g_test_add_func ("/mcp/websocket-server-transport/properties/has-client",
                     test_websocket_server_transport_has_client);
    g_test_add_func ("/mcp/websocket-server-transport/properties/soup-server",
                     test_websocket_server_transport_soup_server);
    g_test_add_func ("/mcp/websocket-server-transport/properties/websocket-connection",
                     test_websocket_server_transport_websocket_connection);
    g_test_add_func ("/mcp/websocket-server-transport/properties/actual-port",
                     test_websocket_server_transport_actual_port);

    /* Interface tests */
    g_test_add_func ("/mcp/websocket-server-transport/interface/initial-state",
                     test_websocket_server_transport_initial_state);
    g_test_add_func ("/mcp/websocket-server-transport/interface/implements-transport",
                     test_websocket_server_transport_interface);

    /* GObject property tests */
    g_test_add_func ("/mcp/websocket-server-transport/gobject/get-properties",
                     test_websocket_server_transport_gobject_properties);
    g_test_add_func ("/mcp/websocket-server-transport/gobject/set-properties",
                     test_websocket_server_transport_gobject_set_properties);

    /* Connect/Disconnect tests */
    g_test_add_func ("/mcp/websocket-server-transport/connect/basic",
                     test_websocket_server_transport_connect);
    g_test_add_func ("/mcp/websocket-server-transport/connect/disconnect",
                     test_websocket_server_transport_disconnect);
    g_test_add_func ("/mcp/websocket-server-transport/connect/double-connect",
                     test_websocket_server_transport_double_connect);
    g_test_add_func ("/mcp/websocket-server-transport/connect/localhost-binding",
                     test_websocket_server_transport_localhost_binding);
    g_test_add_func ("/mcp/websocket-server-transport/connect/custom-path",
                     test_websocket_server_transport_custom_path);

    /* Signal tests */
    g_test_add_func ("/mcp/websocket-server-transport/signals/state-changed",
                     test_websocket_server_transport_state_changed_signal);

    return g_test_run ();
}
