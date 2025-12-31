/*
 * test-capabilities.c - Unit tests for MCP capabilities and session
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include "mcp.h"

/* ========================================================================== */
/* McpImplementation Tests                                                    */
/* ========================================================================== */

/*
 * Test basic implementation creation
 */
static void
test_implementation_new (void)
{
    g_autoptr(McpImplementation) impl = NULL;

    impl = mcp_implementation_new ("test-server", "1.0.0");

    g_assert_nonnull (impl);
    g_assert_cmpstr (mcp_implementation_get_name (impl), ==, "test-server");
    g_assert_cmpstr (mcp_implementation_get_version (impl), ==, "1.0.0");
    g_assert_null (mcp_implementation_get_title (impl));
    g_assert_null (mcp_implementation_get_website_url (impl));
}

/*
 * Test implementation with optional fields
 */
static void
test_implementation_optional_fields (void)
{
    g_autoptr(McpImplementation) impl = NULL;

    impl = mcp_implementation_new ("my-mcp-impl", "2.3.4");
    mcp_implementation_set_title (impl, "My MCP Implementation");
    mcp_implementation_set_website_url (impl, "https://example.com");

    g_assert_cmpstr (mcp_implementation_get_title (impl), ==, "My MCP Implementation");
    g_assert_cmpstr (mcp_implementation_get_website_url (impl), ==, "https://example.com");
}

/*
 * Test implementation JSON serialization
 */
static void
test_implementation_to_json (void)
{
    g_autoptr(McpImplementation) impl = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    impl = mcp_implementation_new ("json-test", "0.1.0");
    mcp_implementation_set_title (impl, "JSON Test");

    json = mcp_implementation_to_json (impl);

    g_assert_nonnull (json);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (json));

    obj = json_node_get_object (json);

    g_assert_cmpstr (json_object_get_string_member (obj, "name"), ==, "json-test");
    g_assert_cmpstr (json_object_get_string_member (obj, "version"), ==, "0.1.0");
    g_assert_cmpstr (json_object_get_string_member (obj, "title"), ==, "JSON Test");
    g_assert_false (json_object_has_member (obj, "websiteUrl"));
}

/*
 * Test implementation JSON deserialization
 */
static void
test_implementation_from_json (void)
{
    g_autoptr(McpImplementation) impl = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str =
        "{"
        "  \"name\": \"parsed-impl\","
        "  \"version\": \"3.2.1\","
        "  \"title\": \"Parsed Implementation\","
        "  \"websiteUrl\": \"https://parsed.example.com\""
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    impl = mcp_implementation_new_from_json (root, &error);

    g_assert_no_error (error);
    g_assert_nonnull (impl);

    g_assert_cmpstr (mcp_implementation_get_name (impl), ==, "parsed-impl");
    g_assert_cmpstr (mcp_implementation_get_version (impl), ==, "3.2.1");
    g_assert_cmpstr (mcp_implementation_get_title (impl), ==, "Parsed Implementation");
    g_assert_cmpstr (mcp_implementation_get_website_url (impl), ==, "https://parsed.example.com");
}

/*
 * Test implementation round-trip
 */
static void
test_implementation_roundtrip (void)
{
    g_autoptr(McpImplementation) original = NULL;
    g_autoptr(McpImplementation) restored = NULL;
    g_autoptr(JsonNode) json = NULL;
    g_autoptr(GError) error = NULL;

    original = mcp_implementation_new ("roundtrip-test", "1.2.3");
    mcp_implementation_set_title (original, "Roundtrip Test");
    mcp_implementation_set_website_url (original, "https://roundtrip.test");

    json = mcp_implementation_to_json (original);
    restored = mcp_implementation_new_from_json (json, &error);

    g_assert_no_error (error);
    g_assert_nonnull (restored);

    g_assert_cmpstr (mcp_implementation_get_name (original), ==,
                     mcp_implementation_get_name (restored));
    g_assert_cmpstr (mcp_implementation_get_version (original), ==,
                     mcp_implementation_get_version (restored));
    g_assert_cmpstr (mcp_implementation_get_title (original), ==,
                     mcp_implementation_get_title (restored));
    g_assert_cmpstr (mcp_implementation_get_website_url (original), ==,
                     mcp_implementation_get_website_url (restored));
}

/* ========================================================================== */
/* McpServerCapabilities Tests                                                */
/* ========================================================================== */

/*
 * Test basic server capabilities creation
 */
static void
test_server_capabilities_new (void)
{
    g_autoptr(McpServerCapabilities) caps = NULL;

    caps = mcp_server_capabilities_new ();

    g_assert_nonnull (caps);
    g_assert_false (mcp_server_capabilities_get_logging (caps));
    g_assert_false (mcp_server_capabilities_get_prompts (caps));
    g_assert_false (mcp_server_capabilities_get_resources (caps));
    g_assert_false (mcp_server_capabilities_get_tools (caps));
    g_assert_false (mcp_server_capabilities_get_completions (caps));
    g_assert_false (mcp_server_capabilities_get_tasks (caps));
}

/*
 * Test server capabilities setters
 */
static void
test_server_capabilities_setters (void)
{
    g_autoptr(McpServerCapabilities) caps = NULL;

    caps = mcp_server_capabilities_new ();

    mcp_server_capabilities_set_logging (caps, TRUE);
    g_assert_true (mcp_server_capabilities_get_logging (caps));

    mcp_server_capabilities_set_prompts (caps, TRUE, TRUE);
    g_assert_true (mcp_server_capabilities_get_prompts (caps));
    g_assert_true (mcp_server_capabilities_get_prompts_list_changed (caps));

    mcp_server_capabilities_set_resources (caps, TRUE, TRUE, FALSE);
    g_assert_true (mcp_server_capabilities_get_resources (caps));
    g_assert_true (mcp_server_capabilities_get_resources_subscribe (caps));
    g_assert_false (mcp_server_capabilities_get_resources_list_changed (caps));

    mcp_server_capabilities_set_tools (caps, TRUE, TRUE);
    g_assert_true (mcp_server_capabilities_get_tools (caps));
    g_assert_true (mcp_server_capabilities_get_tools_list_changed (caps));

    mcp_server_capabilities_set_completions (caps, TRUE);
    g_assert_true (mcp_server_capabilities_get_completions (caps));

    mcp_server_capabilities_set_tasks (caps, TRUE);
    g_assert_true (mcp_server_capabilities_get_tasks (caps));
}

/*
 * Test server capabilities experimental
 */
static void
test_server_capabilities_experimental (void)
{
    g_autoptr(McpServerCapabilities) caps = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *config;

    caps = mcp_server_capabilities_new ();

    /* Set without config */
    mcp_server_capabilities_set_experimental (caps, "feature1", NULL);
    g_assert_nonnull (mcp_server_capabilities_get_experimental (caps, "feature1"));

    /* Set with config */
    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "enabled");
    json_builder_add_boolean_value (builder, TRUE);
    json_builder_end_object (builder);
    config = json_builder_get_root (builder);

    mcp_server_capabilities_set_experimental (caps, "feature2", config);
    g_assert_nonnull (mcp_server_capabilities_get_experimental (caps, "feature2"));

    json_node_unref (config);

    /* Non-existent feature */
    g_assert_null (mcp_server_capabilities_get_experimental (caps, "nonexistent"));
}

/*
 * Test server capabilities JSON serialization
 */
static void
test_server_capabilities_to_json (void)
{
    g_autoptr(McpServerCapabilities) caps = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    caps = mcp_server_capabilities_new ();
    mcp_server_capabilities_set_logging (caps, TRUE);
    mcp_server_capabilities_set_tools (caps, TRUE, TRUE);
    mcp_server_capabilities_set_resources (caps, TRUE, TRUE, TRUE);

    json = mcp_server_capabilities_to_json (caps);

    g_assert_nonnull (json);
    obj = json_node_get_object (json);

    /* Logging should be an empty marker object */
    g_assert_true (json_object_has_member (obj, "logging"));

    /* Tools with listChanged */
    g_assert_true (json_object_has_member (obj, "tools"));
    JsonObject *tools_obj = json_object_get_object_member (obj, "tools");
    g_assert_true (json_object_get_boolean_member (tools_obj, "listChanged"));

    /* Resources with subscribe and listChanged */
    g_assert_true (json_object_has_member (obj, "resources"));
    JsonObject *resources_obj = json_object_get_object_member (obj, "resources");
    g_assert_true (json_object_get_boolean_member (resources_obj, "subscribe"));
    g_assert_true (json_object_get_boolean_member (resources_obj, "listChanged"));

    /* Prompts should not be present (not set) */
    g_assert_false (json_object_has_member (obj, "prompts"));
}

/*
 * Test server capabilities JSON deserialization
 */
static void
test_server_capabilities_from_json (void)
{
    g_autoptr(McpServerCapabilities) caps = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str =
        "{"
        "  \"logging\": {},"
        "  \"prompts\": { \"listChanged\": true },"
        "  \"resources\": { \"subscribe\": true, \"listChanged\": false },"
        "  \"experimental\": { \"myFeature\": { \"key\": \"value\" } }"
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    caps = mcp_server_capabilities_new_from_json (root, &error);

    g_assert_no_error (error);
    g_assert_nonnull (caps);

    g_assert_true (mcp_server_capabilities_get_logging (caps));
    g_assert_true (mcp_server_capabilities_get_prompts (caps));
    g_assert_true (mcp_server_capabilities_get_prompts_list_changed (caps));
    g_assert_true (mcp_server_capabilities_get_resources (caps));
    g_assert_true (mcp_server_capabilities_get_resources_subscribe (caps));
    g_assert_false (mcp_server_capabilities_get_resources_list_changed (caps));
    g_assert_false (mcp_server_capabilities_get_tools (caps));

    g_assert_nonnull (mcp_server_capabilities_get_experimental (caps, "myFeature"));
}

/* ========================================================================== */
/* McpClientCapabilities Tests                                                */
/* ========================================================================== */

/*
 * Test basic client capabilities creation
 */
static void
test_client_capabilities_new (void)
{
    g_autoptr(McpClientCapabilities) caps = NULL;

    caps = mcp_client_capabilities_new ();

    g_assert_nonnull (caps);
    g_assert_false (mcp_client_capabilities_get_sampling (caps));
    g_assert_false (mcp_client_capabilities_get_roots (caps));
    g_assert_false (mcp_client_capabilities_get_elicitation (caps));
    g_assert_false (mcp_client_capabilities_get_tasks (caps));
}

/*
 * Test client capabilities setters
 */
static void
test_client_capabilities_setters (void)
{
    g_autoptr(McpClientCapabilities) caps = NULL;

    caps = mcp_client_capabilities_new ();

    mcp_client_capabilities_set_sampling (caps, TRUE);
    g_assert_true (mcp_client_capabilities_get_sampling (caps));

    mcp_client_capabilities_set_roots (caps, TRUE, TRUE);
    g_assert_true (mcp_client_capabilities_get_roots (caps));
    g_assert_true (mcp_client_capabilities_get_roots_list_changed (caps));

    mcp_client_capabilities_set_elicitation (caps, TRUE);
    g_assert_true (mcp_client_capabilities_get_elicitation (caps));

    mcp_client_capabilities_set_tasks (caps, TRUE);
    g_assert_true (mcp_client_capabilities_get_tasks (caps));
}

/*
 * Test client capabilities JSON serialization
 */
static void
test_client_capabilities_to_json (void)
{
    g_autoptr(McpClientCapabilities) caps = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    caps = mcp_client_capabilities_new ();
    mcp_client_capabilities_set_sampling (caps, TRUE);
    mcp_client_capabilities_set_roots (caps, TRUE, TRUE);

    json = mcp_client_capabilities_to_json (caps);

    g_assert_nonnull (json);
    obj = json_node_get_object (json);

    /* Sampling should be an empty marker object */
    g_assert_true (json_object_has_member (obj, "sampling"));

    /* Roots with listChanged */
    g_assert_true (json_object_has_member (obj, "roots"));
    JsonObject *roots_obj = json_object_get_object_member (obj, "roots");
    g_assert_true (json_object_get_boolean_member (roots_obj, "listChanged"));
}

/*
 * Test client capabilities JSON deserialization
 */
static void
test_client_capabilities_from_json (void)
{
    g_autoptr(McpClientCapabilities) caps = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str =
        "{"
        "  \"sampling\": {},"
        "  \"roots\": { \"listChanged\": true },"
        "  \"elicitation\": {},"
        "  \"tasks\": {}"
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    caps = mcp_client_capabilities_new_from_json (root, &error);

    g_assert_no_error (error);
    g_assert_nonnull (caps);

    g_assert_true (mcp_client_capabilities_get_sampling (caps));
    g_assert_true (mcp_client_capabilities_get_roots (caps));
    g_assert_true (mcp_client_capabilities_get_roots_list_changed (caps));
    g_assert_true (mcp_client_capabilities_get_elicitation (caps));
    g_assert_true (mcp_client_capabilities_get_tasks (caps));
}

/* ========================================================================== */
/* McpSession Tests                                                           */
/* ========================================================================== */

/*
 * Test session initial state
 */
static void
test_session_initial_state (void)
{
    g_autoptr(McpSession) session = NULL;

    session = g_object_new (MCP_TYPE_SESSION, NULL);

    g_assert_nonnull (session);
    g_assert_cmpint (mcp_session_get_state (session), ==, MCP_SESSION_STATE_DISCONNECTED);
    g_assert_null (mcp_session_get_protocol_version (session));
    g_assert_null (mcp_session_get_local_implementation (session));
    g_assert_null (mcp_session_get_remote_implementation (session));
}

/*
 * Test session request ID generation
 */
static void
test_session_request_id (void)
{
    g_autoptr(McpSession) session = NULL;
    g_autofree gchar *id1 = NULL;
    g_autofree gchar *id2 = NULL;
    g_autofree gchar *id3 = NULL;

    session = g_object_new (MCP_TYPE_SESSION, NULL);

    id1 = mcp_session_generate_request_id (session);
    id2 = mcp_session_generate_request_id (session);
    id3 = mcp_session_generate_request_id (session);

    g_assert_nonnull (id1);
    g_assert_nonnull (id2);
    g_assert_nonnull (id3);

    /* All IDs should be unique */
    g_assert_cmpstr (id1, !=, id2);
    g_assert_cmpstr (id2, !=, id3);
    g_assert_cmpstr (id1, !=, id3);
}

/*
 * Test session implementation info
 */
static void
test_session_implementation (void)
{
    g_autoptr(McpSession) session = NULL;
    g_autoptr(McpImplementation) impl = NULL;

    session = g_object_new (MCP_TYPE_SESSION, NULL);
    impl = mcp_implementation_new ("test-session", "1.0.0");

    mcp_session_set_local_implementation (session, impl);

    g_assert_nonnull (mcp_session_get_local_implementation (session));
    g_assert_cmpstr (mcp_implementation_get_name (mcp_session_get_local_implementation (session)),
                     ==, "test-session");
}

/*
 * Test session pending requests
 */
static void
test_session_pending_requests (void)
{
    g_autoptr(McpSession) session = NULL;

    session = g_object_new (MCP_TYPE_SESSION, NULL);

    /* Initially no pending requests */
    g_assert_cmpuint (mcp_session_get_pending_request_count (session), ==, 0);
    g_assert_false (mcp_session_has_pending_request (session, "1"));
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* McpImplementation tests */
    g_test_add_func ("/mcp/implementation/new", test_implementation_new);
    g_test_add_func ("/mcp/implementation/optional-fields", test_implementation_optional_fields);
    g_test_add_func ("/mcp/implementation/to-json", test_implementation_to_json);
    g_test_add_func ("/mcp/implementation/from-json", test_implementation_from_json);
    g_test_add_func ("/mcp/implementation/roundtrip", test_implementation_roundtrip);

    /* McpServerCapabilities tests */
    g_test_add_func ("/mcp/server-capabilities/new", test_server_capabilities_new);
    g_test_add_func ("/mcp/server-capabilities/setters", test_server_capabilities_setters);
    g_test_add_func ("/mcp/server-capabilities/experimental", test_server_capabilities_experimental);
    g_test_add_func ("/mcp/server-capabilities/to-json", test_server_capabilities_to_json);
    g_test_add_func ("/mcp/server-capabilities/from-json", test_server_capabilities_from_json);

    /* McpClientCapabilities tests */
    g_test_add_func ("/mcp/client-capabilities/new", test_client_capabilities_new);
    g_test_add_func ("/mcp/client-capabilities/setters", test_client_capabilities_setters);
    g_test_add_func ("/mcp/client-capabilities/to-json", test_client_capabilities_to_json);
    g_test_add_func ("/mcp/client-capabilities/from-json", test_client_capabilities_from_json);

    /* McpSession tests */
    g_test_add_func ("/mcp/session/initial-state", test_session_initial_state);
    g_test_add_func ("/mcp/session/request-id", test_session_request_id);
    g_test_add_func ("/mcp/session/implementation", test_session_implementation);
    g_test_add_func ("/mcp/session/pending-requests", test_session_pending_requests);

    return g_test_run ();
}
