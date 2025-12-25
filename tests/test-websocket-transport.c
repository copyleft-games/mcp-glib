/*
 * test-websocket-transport.c - Tests for McpWebSocketTransport
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include <mcp/mcp.h>

/* Test WebSocket transport creation */
static void
test_websocket_transport_new (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");
    g_assert_nonnull (transport);
    g_assert_true (MCP_IS_WEBSOCKET_TRANSPORT (transport));
    g_assert_true (MCP_IS_TRANSPORT (transport));
}

/* Test secure WebSocket URI */
static void
test_websocket_transport_new_wss (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;

    transport = mcp_websocket_transport_new ("wss://localhost:8443/mcp");
    g_assert_nonnull (transport);
    g_assert_cmpstr (mcp_websocket_transport_get_uri (transport), ==, "wss://localhost:8443/mcp");
}

/* Test URI */
static void
test_websocket_transport_uri (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");

    g_assert_cmpstr (mcp_websocket_transport_get_uri (transport), ==, "ws://localhost:8080/mcp");

    mcp_websocket_transport_set_uri (transport, "ws://example.com/api");
    g_assert_cmpstr (mcp_websocket_transport_get_uri (transport), ==, "ws://example.com/api");
}

/* Test auth token */
static void
test_websocket_transport_auth_token (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");

    g_assert_null (mcp_websocket_transport_get_auth_token (transport));

    mcp_websocket_transport_set_auth_token (transport, "test-token-456");
    g_assert_cmpstr (mcp_websocket_transport_get_auth_token (transport), ==, "test-token-456");

    mcp_websocket_transport_set_auth_token (transport, NULL);
    g_assert_null (mcp_websocket_transport_get_auth_token (transport));
}

/* Test protocols */
static void
test_websocket_transport_protocols (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;
    const gchar * const *protocols;
    const gchar *test_protocols[] = { "mcp", "mcp-v1", NULL };

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");

    g_assert_null (mcp_websocket_transport_get_protocols (transport));

    mcp_websocket_transport_set_protocols (transport, test_protocols);
    protocols = mcp_websocket_transport_get_protocols (transport);

    g_assert_nonnull (protocols);
    g_assert_cmpstr (protocols[0], ==, "mcp");
    g_assert_cmpstr (protocols[1], ==, "mcp-v1");
    g_assert_null (protocols[2]);
}

/* Test origin */
static void
test_websocket_transport_origin (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");

    g_assert_null (mcp_websocket_transport_get_origin (transport));

    mcp_websocket_transport_set_origin (transport, "http://localhost:3000");
    g_assert_cmpstr (mcp_websocket_transport_get_origin (transport), ==, "http://localhost:3000");
}

/* Test reconnect settings */
static void
test_websocket_transport_reconnect (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");

    /* Default: reconnect enabled */
    g_assert_true (mcp_websocket_transport_get_reconnect_enabled (transport));

    mcp_websocket_transport_set_reconnect_enabled (transport, FALSE);
    g_assert_false (mcp_websocket_transport_get_reconnect_enabled (transport));

    /* Default delay */
    g_assert_cmpuint (mcp_websocket_transport_get_reconnect_delay (transport), ==, 1000);

    mcp_websocket_transport_set_reconnect_delay (transport, 2000);
    g_assert_cmpuint (mcp_websocket_transport_get_reconnect_delay (transport), ==, 2000);
}

/* Test max reconnect attempts */
static void
test_websocket_transport_max_reconnect (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");

    /* Default: unlimited (0) */
    g_assert_cmpuint (mcp_websocket_transport_get_max_reconnect_attempts (transport), ==, 0);

    mcp_websocket_transport_set_max_reconnect_attempts (transport, 5);
    g_assert_cmpuint (mcp_websocket_transport_get_max_reconnect_attempts (transport), ==, 5);
}

/* Test keepalive interval */
static void
test_websocket_transport_keepalive (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");

    /* Default keepalive */
    g_assert_cmpuint (mcp_websocket_transport_get_keepalive_interval (transport), ==, 30);

    mcp_websocket_transport_set_keepalive_interval (transport, 60);
    g_assert_cmpuint (mcp_websocket_transport_get_keepalive_interval (transport), ==, 60);

    /* Disable keepalive */
    mcp_websocket_transport_set_keepalive_interval (transport, 0);
    g_assert_cmpuint (mcp_websocket_transport_get_keepalive_interval (transport), ==, 0);
}

/* Test initial state */
static void
test_websocket_transport_initial_state (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");

    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (transport)), ==,
                     MCP_TRANSPORT_STATE_DISCONNECTED);
    g_assert_false (mcp_transport_is_connected (MCP_TRANSPORT (transport)));
}

/* Test soup session accessor */
static void
test_websocket_transport_soup_session (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;
    SoupSession *session;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");
    session = mcp_websocket_transport_get_soup_session (transport);

    g_assert_nonnull (session);
    g_assert_true (SOUP_IS_SESSION (session));
}

/* Test WebSocket connection accessor (null before connect) */
static void
test_websocket_transport_connection (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");

    /* No connection before connecting */
    g_assert_null (mcp_websocket_transport_get_websocket_connection (transport));
}

/* Test with custom session */
static void
test_websocket_transport_with_session (void)
{
    g_autoptr(SoupSession) session = NULL;
    g_autoptr(McpWebSocketTransport) transport = NULL;

    session = soup_session_new ();
    transport = mcp_websocket_transport_new_with_session ("ws://localhost:8080/mcp", session);

    g_assert_nonnull (transport);
    g_assert_true (mcp_websocket_transport_get_soup_session (transport) == session);
}

/* Test properties via GObject */
static void
test_websocket_transport_properties (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;
    g_autofree gchar *uri = NULL;
    g_autofree gchar *auth_token = NULL;
    g_autofree gchar *origin = NULL;
    guint reconnect_delay;
    guint max_attempts;
    guint keepalive;
    gboolean reconnect_enabled;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");

    mcp_websocket_transport_set_auth_token (transport, "my-token");
    mcp_websocket_transport_set_origin (transport, "http://myapp.com");
    mcp_websocket_transport_set_reconnect_delay (transport, 2500);
    mcp_websocket_transport_set_max_reconnect_attempts (transport, 10);
    mcp_websocket_transport_set_keepalive_interval (transport, 45);
    mcp_websocket_transport_set_reconnect_enabled (transport, FALSE);

    g_object_get (transport,
                  "uri", &uri,
                  "auth-token", &auth_token,
                  "origin", &origin,
                  "reconnect-delay", &reconnect_delay,
                  "max-reconnect-attempts", &max_attempts,
                  "keepalive-interval", &keepalive,
                  "reconnect-enabled", &reconnect_enabled,
                  NULL);

    g_assert_cmpstr (uri, ==, "ws://localhost:8080/mcp");
    g_assert_cmpstr (auth_token, ==, "my-token");
    g_assert_cmpstr (origin, ==, "http://myapp.com");
    g_assert_cmpuint (reconnect_delay, ==, 2500);
    g_assert_cmpuint (max_attempts, ==, 10);
    g_assert_cmpuint (keepalive, ==, 45);
    g_assert_false (reconnect_enabled);
}

/* Test transport interface implementation */
static void
test_websocket_transport_interface (void)
{
    g_autoptr(McpWebSocketTransport) transport = NULL;
    McpTransportInterface *iface;

    transport = mcp_websocket_transport_new ("ws://localhost:8080/mcp");
    iface = MCP_TRANSPORT_GET_IFACE (transport);

    g_assert_nonnull (iface->get_state);
    g_assert_nonnull (iface->connect_async);
    g_assert_nonnull (iface->connect_finish);
    g_assert_nonnull (iface->disconnect_async);
    g_assert_nonnull (iface->disconnect_finish);
    g_assert_nonnull (iface->send_message_async);
    g_assert_nonnull (iface->send_message_finish);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/transport/websocket/new", test_websocket_transport_new);
    g_test_add_func ("/transport/websocket/new-wss", test_websocket_transport_new_wss);
    g_test_add_func ("/transport/websocket/uri", test_websocket_transport_uri);
    g_test_add_func ("/transport/websocket/auth-token", test_websocket_transport_auth_token);
    g_test_add_func ("/transport/websocket/protocols", test_websocket_transport_protocols);
    g_test_add_func ("/transport/websocket/origin", test_websocket_transport_origin);
    g_test_add_func ("/transport/websocket/reconnect", test_websocket_transport_reconnect);
    g_test_add_func ("/transport/websocket/max-reconnect", test_websocket_transport_max_reconnect);
    g_test_add_func ("/transport/websocket/keepalive", test_websocket_transport_keepalive);
    g_test_add_func ("/transport/websocket/initial-state", test_websocket_transport_initial_state);
    g_test_add_func ("/transport/websocket/soup-session", test_websocket_transport_soup_session);
    g_test_add_func ("/transport/websocket/connection", test_websocket_transport_connection);
    g_test_add_func ("/transport/websocket/with-session", test_websocket_transport_with_session);
    g_test_add_func ("/transport/websocket/properties", test_websocket_transport_properties);
    g_test_add_func ("/transport/websocket/interface", test_websocket_transport_interface);

    return g_test_run ();
}
