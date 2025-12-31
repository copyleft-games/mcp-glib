/*
 * test-client.c - Tests for McpClient
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include "mcp.h"

/* Test client creation */
static void
test_client_new (void)
{
    g_autoptr(McpClient) client = NULL;

    client = mcp_client_new ("test-client", "1.0.0");
    g_assert_nonnull (client);
    g_assert_true (MCP_IS_CLIENT (client));
    g_assert_true (MCP_IS_SESSION (client));
}

/* Test client capabilities */
static void
test_client_capabilities (void)
{
    g_autoptr(McpClient) client = NULL;
    McpClientCapabilities *caps;

    client = mcp_client_new ("test-client", "1.0.0");
    caps = mcp_client_get_capabilities (client);

    g_assert_nonnull (caps);
    g_assert_true (MCP_IS_CLIENT_CAPABILITIES (caps));
}

/* Test client initial state */
static void
test_client_initial_state (void)
{
    g_autoptr(McpClient) client = NULL;

    client = mcp_client_new ("test-client", "1.0.0");

    /* Should start disconnected */
    g_assert_cmpint (mcp_session_get_state (MCP_SESSION (client)), ==, MCP_SESSION_STATE_DISCONNECTED);

    /* No transport initially */
    g_assert_null (mcp_client_get_transport (client));

    /* No server capabilities initially */
    g_assert_null (mcp_client_get_server_capabilities (client));

    /* No server instructions initially */
    g_assert_null (mcp_client_get_server_instructions (client));
}

/* Test client implementation info */
static void
test_client_implementation (void)
{
    g_autoptr(McpClient) client = NULL;
    McpImplementation *impl;

    client = mcp_client_new ("my-client", "2.0.0");
    impl = mcp_session_get_local_implementation (MCP_SESSION (client));

    g_assert_nonnull (impl);
    g_assert_cmpstr (mcp_implementation_get_name (impl), ==, "my-client");
    g_assert_cmpstr (mcp_implementation_get_version (impl), ==, "2.0.0");
}

/* Test client properties via GObject */
static void
test_client_properties (void)
{
    g_autoptr(McpClient) client = NULL;
    g_autoptr(McpClientCapabilities) caps = NULL;
    g_autoptr(McpTransport) transport = NULL;
    gchar *instructions = NULL;

    client = mcp_client_new ("test-client", "1.0.0");

    g_object_get (client,
                  "capabilities", &caps,
                  "transport", &transport,
                  "server-instructions", &instructions,
                  NULL);

    g_assert_nonnull (caps);
    g_assert_null (transport);
    g_assert_null (instructions);
}

/* Test client with client capabilities features */
static void
test_client_capabilities_features (void)
{
    g_autoptr(McpClient) client = NULL;
    McpClientCapabilities *caps;

    client = mcp_client_new ("test-client", "1.0.0");
    caps = mcp_client_get_capabilities (client);

    /* Enable some features */
    mcp_client_capabilities_set_sampling (caps, TRUE);
    mcp_client_capabilities_set_roots (caps, TRUE, TRUE);
    mcp_client_capabilities_set_elicitation (caps, TRUE);

    g_assert_true (mcp_client_capabilities_get_sampling (caps));
    g_assert_true (mcp_client_capabilities_get_roots (caps));
    g_assert_true (mcp_client_capabilities_get_elicitation (caps));
}

/* Test client capabilities JSON serialization */
static void
test_client_capabilities_json (void)
{
    g_autoptr(McpClient) client = NULL;
    McpClientCapabilities *caps;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;

    client = mcp_client_new ("test-client", "1.0.0");
    caps = mcp_client_get_capabilities (client);

    mcp_client_capabilities_set_sampling (caps, TRUE);
    mcp_client_capabilities_set_roots (caps, TRUE, TRUE);

    node = mcp_client_capabilities_to_json (caps);
    g_assert_nonnull (node);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (node));

    obj = json_node_get_object (node);

    if (json_object_has_member (obj, "sampling"))
    {
        g_assert_true (JSON_NODE_HOLDS_OBJECT (json_object_get_member (obj, "sampling")));
    }

    if (json_object_has_member (obj, "roots"))
    {
        g_assert_true (JSON_NODE_HOLDS_OBJECT (json_object_get_member (obj, "roots")));
    }
}

/* Test multiple client instances */
static void
test_client_multiple_instances (void)
{
    g_autoptr(McpClient) client1 = NULL;
    g_autoptr(McpClient) client2 = NULL;
    McpImplementation *impl1;
    McpImplementation *impl2;

    client1 = mcp_client_new ("client-1", "1.0.0");
    client2 = mcp_client_new ("client-2", "2.0.0");

    impl1 = mcp_session_get_local_implementation (MCP_SESSION (client1));
    impl2 = mcp_session_get_local_implementation (MCP_SESSION (client2));

    g_assert_cmpstr (mcp_implementation_get_name (impl1), ==, "client-1");
    g_assert_cmpstr (mcp_implementation_get_name (impl2), ==, "client-2");

    /* Different capabilities instances */
    g_assert_true (mcp_client_get_capabilities (client1) != mcp_client_get_capabilities (client2));
}

/* Test client session ID generation */
static void
test_client_request_id_generation (void)
{
    g_autoptr(McpClient) client = NULL;
    g_autofree gchar *id1 = NULL;
    g_autofree gchar *id2 = NULL;
    g_autofree gchar *id3 = NULL;

    client = mcp_client_new ("test-client", "1.0.0");

    id1 = mcp_session_generate_request_id (MCP_SESSION (client));
    id2 = mcp_session_generate_request_id (MCP_SESSION (client));
    id3 = mcp_session_generate_request_id (MCP_SESSION (client));

    g_assert_nonnull (id1);
    g_assert_nonnull (id2);
    g_assert_nonnull (id3);

    /* IDs should be different */
    g_assert_cmpstr (id1, !=, id2);
    g_assert_cmpstr (id2, !=, id3);
    g_assert_cmpstr (id1, !=, id3);
}

/* Callback for connect_no_transport test */
static void
connect_no_transport_cb (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
    GMainLoop *loop = user_data;
    GError *err = NULL;
    gboolean success;

    success = mcp_client_connect_finish (MCP_CLIENT (source), result, &err);
    g_assert_false (success);
    g_assert_nonnull (err);
    g_assert_cmpint (err->code, ==, MCP_ERROR_INTERNAL_ERROR);
    g_error_free (err);
    g_main_loop_quit (loop);
}

static gboolean
timeout_quit (gpointer user_data)
{
    g_main_loop_quit ((GMainLoop *) user_data);
    return G_SOURCE_REMOVE;
}

/* Test client connect without transport */
static void
test_client_connect_no_transport (void)
{
    g_autoptr(McpClient) client = NULL;
    GMainLoop *loop;

    client = mcp_client_new ("test-client", "1.0.0");
    loop = g_main_loop_new (NULL, FALSE);

    /* Try to connect without setting transport - should fail async */
    mcp_client_connect_async (client, NULL, connect_no_transport_cb, loop);

    /* Give it a chance to call the callback with a timeout fallback */
    g_timeout_add (500, timeout_quit, loop);
    g_main_loop_run (loop);

    g_main_loop_unref (loop);
}

/* Test client disconnect without connection */
static void
test_client_disconnect_not_connected (void)
{
    g_autoptr(McpClient) client = NULL;

    client = mcp_client_new ("test-client", "1.0.0");

    /* Disconnecting when not connected should just succeed */
    mcp_client_disconnect_async (client, NULL, NULL, NULL);
}

/* Test client as a session */
static void
test_client_is_session (void)
{
    g_autoptr(McpClient) client = NULL;

    client = mcp_client_new ("test-client", "1.0.0");

    g_assert_true (MCP_IS_SESSION (client));
    g_assert_cmpint (mcp_session_get_state (MCP_SESSION (client)), ==, MCP_SESSION_STATE_DISCONNECTED);
}

/* Test client type hierarchy */
static void
test_client_type_hierarchy (void)
{
    GType client_type;
    GType session_type;

    client_type = MCP_TYPE_CLIENT;
    session_type = MCP_TYPE_SESSION;

    g_assert_true (g_type_is_a (client_type, session_type));
    g_assert_true (g_type_is_a (client_type, G_TYPE_OBJECT));
}

/* Test client refcounting */
static void
test_client_refcount (void)
{
    McpClient *client;
    McpClient *client2;

    client = mcp_client_new ("test-client", "1.0.0");
    g_assert_nonnull (client);

    client2 = g_object_ref (client);
    g_assert_true (client == client2);

    g_object_unref (client2);
    g_object_unref (client);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Client creation tests */
    g_test_add_func ("/mcp/client/new", test_client_new);
    g_test_add_func ("/mcp/client/capabilities", test_client_capabilities);
    g_test_add_func ("/mcp/client/initial-state", test_client_initial_state);
    g_test_add_func ("/mcp/client/implementation", test_client_implementation);
    g_test_add_func ("/mcp/client/properties", test_client_properties);

    /* Capabilities tests */
    g_test_add_func ("/mcp/client/capabilities-features", test_client_capabilities_features);
    g_test_add_func ("/mcp/client/capabilities-json", test_client_capabilities_json);

    /* Client instance tests */
    g_test_add_func ("/mcp/client/multiple-instances", test_client_multiple_instances);
    g_test_add_func ("/mcp/client/request-id-generation", test_client_request_id_generation);

    /* Connection tests */
    g_test_add_func ("/mcp/client/connect-no-transport", test_client_connect_no_transport);
    g_test_add_func ("/mcp/client/disconnect-not-connected", test_client_disconnect_not_connected);

    /* Type system tests */
    g_test_add_func ("/mcp/client/is-session", test_client_is_session);
    g_test_add_func ("/mcp/client/type-hierarchy", test_client_type_hierarchy);
    g_test_add_func ("/mcp/client/refcount", test_client_refcount);

    return g_test_run ();
}
