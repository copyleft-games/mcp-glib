/*
 * test-http-transport.c - Tests for McpHttpTransport
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include "mcp.h"

/* Test HTTP transport creation */
static void
test_http_transport_new (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");
    g_assert_nonnull (transport);
    g_assert_true (MCP_IS_HTTP_TRANSPORT (transport));
    g_assert_true (MCP_IS_TRANSPORT (transport));
}

/* Test base URL */
static void
test_http_transport_base_url (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");

    g_assert_cmpstr (mcp_http_transport_get_base_url (transport), ==, "http://localhost:8080/mcp");

    mcp_http_transport_set_base_url (transport, "http://example.com/api");
    g_assert_cmpstr (mcp_http_transport_get_base_url (transport), ==, "http://example.com/api");
}

/* Test auth token */
static void
test_http_transport_auth_token (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");

    g_assert_null (mcp_http_transport_get_auth_token (transport));

    mcp_http_transport_set_auth_token (transport, "test-token-123");
    g_assert_cmpstr (mcp_http_transport_get_auth_token (transport), ==, "test-token-123");

    mcp_http_transport_set_auth_token (transport, NULL);
    g_assert_null (mcp_http_transport_get_auth_token (transport));
}

/* Test session ID (read-only, set by server) */
static void
test_http_transport_session_id (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");

    /* Session ID is NULL before connection */
    g_assert_null (mcp_http_transport_get_session_id (transport));
}

/* Test timeout settings */
static void
test_http_transport_timeout (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");

    /* Default timeout */
    g_assert_cmpuint (mcp_http_transport_get_timeout (transport), ==, 30);

    mcp_http_transport_set_timeout (transport, 60);
    g_assert_cmpuint (mcp_http_transport_get_timeout (transport), ==, 60);

    mcp_http_transport_set_timeout (transport, 0);
    g_assert_cmpuint (mcp_http_transport_get_timeout (transport), ==, 0);
}

/* Test SSE endpoint */
static void
test_http_transport_sse_endpoint (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");

    g_assert_null (mcp_http_transport_get_sse_endpoint (transport));

    mcp_http_transport_set_sse_endpoint (transport, "/events");
    g_assert_cmpstr (mcp_http_transport_get_sse_endpoint (transport), ==, "/events");
}

/* Test POST endpoint */
static void
test_http_transport_post_endpoint (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");

    g_assert_null (mcp_http_transport_get_post_endpoint (transport));

    mcp_http_transport_set_post_endpoint (transport, "/messages");
    g_assert_cmpstr (mcp_http_transport_get_post_endpoint (transport), ==, "/messages");
}

/* Test reconnect settings */
static void
test_http_transport_reconnect (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");

    /* Default: reconnect enabled */
    g_assert_true (mcp_http_transport_get_reconnect_enabled (transport));

    mcp_http_transport_set_reconnect_enabled (transport, FALSE);
    g_assert_false (mcp_http_transport_get_reconnect_enabled (transport));

    /* Default delay */
    g_assert_cmpuint (mcp_http_transport_get_reconnect_delay (transport), ==, 3000);

    mcp_http_transport_set_reconnect_delay (transport, 5000);
    g_assert_cmpuint (mcp_http_transport_get_reconnect_delay (transport), ==, 5000);
}

/* Test initial state */
static void
test_http_transport_initial_state (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");

    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (transport)), ==,
                     MCP_TRANSPORT_STATE_DISCONNECTED);
    g_assert_false (mcp_transport_is_connected (MCP_TRANSPORT (transport)));
}

/* Test soup session accessor */
static void
test_http_transport_soup_session (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;
    SoupSession *session;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");
    session = mcp_http_transport_get_soup_session (transport);

    g_assert_nonnull (session);
    g_assert_true (SOUP_IS_SESSION (session));
}

/* Test with custom session */
static void
test_http_transport_with_session (void)
{
    g_autoptr(SoupSession) session = NULL;
    g_autoptr(McpHttpTransport) transport = NULL;

    session = soup_session_new ();
    transport = mcp_http_transport_new_with_session ("http://localhost:8080/mcp", session);

    g_assert_nonnull (transport);
    g_assert_true (mcp_http_transport_get_soup_session (transport) == session);
}

/* Test properties via GObject */
static void
test_http_transport_properties (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;
    g_autofree gchar *base_url = NULL;
    g_autofree gchar *auth_token = NULL;
    guint timeout;
    gboolean reconnect_enabled;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");

    mcp_http_transport_set_auth_token (transport, "my-token");
    mcp_http_transport_set_timeout (transport, 45);
    mcp_http_transport_set_reconnect_enabled (transport, FALSE);

    g_object_get (transport,
                  "base-url", &base_url,
                  "auth-token", &auth_token,
                  "timeout", &timeout,
                  "reconnect-enabled", &reconnect_enabled,
                  NULL);

    g_assert_cmpstr (base_url, ==, "http://localhost:8080/mcp");
    g_assert_cmpstr (auth_token, ==, "my-token");
    g_assert_cmpuint (timeout, ==, 45);
    g_assert_false (reconnect_enabled);
}

/* Test transport interface implementation */
static void
test_http_transport_interface (void)
{
    g_autoptr(McpHttpTransport) transport = NULL;
    McpTransportInterface *iface;

    transport = mcp_http_transport_new ("http://localhost:8080/mcp");
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

    g_test_add_func ("/transport/http/new", test_http_transport_new);
    g_test_add_func ("/transport/http/base-url", test_http_transport_base_url);
    g_test_add_func ("/transport/http/auth-token", test_http_transport_auth_token);
    g_test_add_func ("/transport/http/session-id", test_http_transport_session_id);
    g_test_add_func ("/transport/http/timeout", test_http_transport_timeout);
    g_test_add_func ("/transport/http/sse-endpoint", test_http_transport_sse_endpoint);
    g_test_add_func ("/transport/http/post-endpoint", test_http_transport_post_endpoint);
    g_test_add_func ("/transport/http/reconnect", test_http_transport_reconnect);
    g_test_add_func ("/transport/http/initial-state", test_http_transport_initial_state);
    g_test_add_func ("/transport/http/soup-session", test_http_transport_soup_session);
    g_test_add_func ("/transport/http/with-session", test_http_transport_with_session);
    g_test_add_func ("/transport/http/properties", test_http_transport_properties);
    g_test_add_func ("/transport/http/interface", test_http_transport_interface);

    return g_test_run ();
}
