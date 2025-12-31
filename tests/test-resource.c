/*
 * test-resource.c - Unit tests for McpResource and McpResourceTemplate
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include "mcp.h"

/* ========================================================================== */
/* McpResource Tests                                                          */
/* ========================================================================== */

/*
 * Test basic resource creation
 */
static void
test_resource_new (void)
{
    g_autoptr(McpResource) resource = NULL;

    resource = mcp_resource_new ("file:///tmp/test.txt", "test-file");

    g_assert_nonnull (resource);
    g_assert_cmpstr (mcp_resource_get_uri (resource), ==, "file:///tmp/test.txt");
    g_assert_cmpstr (mcp_resource_get_name (resource), ==, "test-file");
    g_assert_null (mcp_resource_get_title (resource));
    g_assert_null (mcp_resource_get_description (resource));
    g_assert_null (mcp_resource_get_mime_type (resource));
    g_assert_cmpint (mcp_resource_get_size (resource), ==, -1);
}

/*
 * Test resource properties
 */
static void
test_resource_properties (void)
{
    g_autoptr(McpResource) resource = NULL;

    resource = mcp_resource_new ("config://settings", "app-settings");

    /* Test title property */
    g_assert_null (mcp_resource_get_title (resource));
    mcp_resource_set_title (resource, "Application Settings");
    g_assert_cmpstr (mcp_resource_get_title (resource), ==, "Application Settings");

    /* Test description property */
    mcp_resource_set_description (resource, "Current application configuration");
    g_assert_cmpstr (mcp_resource_get_description (resource), ==, "Current application configuration");

    /* Test MIME type property */
    mcp_resource_set_mime_type (resource, "application/json");
    g_assert_cmpstr (mcp_resource_get_mime_type (resource), ==, "application/json");

    /* Test size property */
    g_assert_cmpint (mcp_resource_get_size (resource), ==, -1);
    mcp_resource_set_size (resource, 1024);
    g_assert_cmpint (mcp_resource_get_size (resource), ==, 1024);

    /* Test changing uri */
    mcp_resource_set_uri (resource, "config://new-settings");
    g_assert_cmpstr (mcp_resource_get_uri (resource), ==, "config://new-settings");

    /* Test changing name */
    mcp_resource_set_name (resource, "new-settings");
    g_assert_cmpstr (mcp_resource_get_name (resource), ==, "new-settings");
}

/*
 * Test JSON serialization
 */
static void
test_resource_to_json (void)
{
    g_autoptr(McpResource) resource = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    resource = mcp_resource_new ("file:///data/document.pdf", "document");
    mcp_resource_set_title (resource, "Important Document");
    mcp_resource_set_description (resource, "A very important PDF document");
    mcp_resource_set_mime_type (resource, "application/pdf");
    mcp_resource_set_size (resource, 512000);

    json = mcp_resource_to_json (resource);

    g_assert_nonnull (json);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (json));

    obj = json_node_get_object (json);

    g_assert_true (json_object_has_member (obj, "uri"));
    g_assert_cmpstr (json_object_get_string_member (obj, "uri"), ==, "file:///data/document.pdf");

    g_assert_true (json_object_has_member (obj, "name"));
    g_assert_cmpstr (json_object_get_string_member (obj, "name"), ==, "document");

    g_assert_true (json_object_has_member (obj, "title"));
    g_assert_cmpstr (json_object_get_string_member (obj, "title"), ==, "Important Document");

    g_assert_true (json_object_has_member (obj, "description"));
    g_assert_cmpstr (json_object_get_string_member (obj, "description"), ==, "A very important PDF document");

    g_assert_true (json_object_has_member (obj, "mimeType"));
    g_assert_cmpstr (json_object_get_string_member (obj, "mimeType"), ==, "application/pdf");

    g_assert_true (json_object_has_member (obj, "size"));
    g_assert_cmpint (json_object_get_int_member (obj, "size"), ==, 512000);
}

/*
 * Test JSON serialization without optional fields
 */
static void
test_resource_to_json_minimal (void)
{
    g_autoptr(McpResource) resource = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    resource = mcp_resource_new ("db://users/1", "user-1");

    json = mcp_resource_to_json (resource);

    g_assert_nonnull (json);
    obj = json_node_get_object (json);

    /* Required fields should be present */
    g_assert_true (json_object_has_member (obj, "uri"));
    g_assert_true (json_object_has_member (obj, "name"));

    /* Optional fields should not be present when not set */
    g_assert_false (json_object_has_member (obj, "title"));
    g_assert_false (json_object_has_member (obj, "description"));
    g_assert_false (json_object_has_member (obj, "mimeType"));
    g_assert_false (json_object_has_member (obj, "size"));
}

/*
 * Test JSON deserialization
 */
static void
test_resource_from_json (void)
{
    g_autoptr(McpResource) resource = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str =
        "{"
        "  \"uri\": \"file:///tmp/data.json\","
        "  \"name\": \"data-file\","
        "  \"title\": \"Data File\","
        "  \"description\": \"JSON data file\","
        "  \"mimeType\": \"application/json\","
        "  \"size\": 2048"
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    resource = mcp_resource_new_from_json (root, &error);

    g_assert_no_error (error);
    g_assert_nonnull (resource);

    g_assert_cmpstr (mcp_resource_get_uri (resource), ==, "file:///tmp/data.json");
    g_assert_cmpstr (mcp_resource_get_name (resource), ==, "data-file");
    g_assert_cmpstr (mcp_resource_get_title (resource), ==, "Data File");
    g_assert_cmpstr (mcp_resource_get_description (resource), ==, "JSON data file");
    g_assert_cmpstr (mcp_resource_get_mime_type (resource), ==, "application/json");
    g_assert_cmpint (mcp_resource_get_size (resource), ==, 2048);
}

/*
 * Test JSON deserialization with missing uri (should fail)
 */
static void
test_resource_from_json_missing_uri (void)
{
    g_autoptr(McpResource) resource = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str = "{ \"name\": \"no-uri\" }";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    resource = mcp_resource_new_from_json (root, &error);

    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_null (resource);
}

/*
 * Test JSON deserialization with missing name (should fail)
 */
static void
test_resource_from_json_missing_name (void)
{
    g_autoptr(McpResource) resource = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str = "{ \"uri\": \"file:///test\" }";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    resource = mcp_resource_new_from_json (root, &error);

    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_null (resource);
}

/*
 * Test round-trip JSON serialization
 */
static void
test_resource_json_roundtrip (void)
{
    g_autoptr(McpResource) original = NULL;
    g_autoptr(McpResource) restored = NULL;
    g_autoptr(JsonNode) json = NULL;
    g_autoptr(GError) error = NULL;

    original = mcp_resource_new ("https://api.example.com/data", "api-data");
    mcp_resource_set_title (original, "API Data");
    mcp_resource_set_description (original, "Data from the API");
    mcp_resource_set_mime_type (original, "application/json");
    mcp_resource_set_size (original, 4096);

    json = mcp_resource_to_json (original);
    restored = mcp_resource_new_from_json (json, &error);

    g_assert_no_error (error);
    g_assert_nonnull (restored);

    g_assert_cmpstr (mcp_resource_get_uri (original), ==, mcp_resource_get_uri (restored));
    g_assert_cmpstr (mcp_resource_get_name (original), ==, mcp_resource_get_name (restored));
    g_assert_cmpstr (mcp_resource_get_title (original), ==, mcp_resource_get_title (restored));
    g_assert_cmpstr (mcp_resource_get_description (original), ==, mcp_resource_get_description (restored));
    g_assert_cmpstr (mcp_resource_get_mime_type (original), ==, mcp_resource_get_mime_type (restored));
    g_assert_cmpint (mcp_resource_get_size (original), ==, mcp_resource_get_size (restored));
}

/* ========================================================================== */
/* McpResourceTemplate Tests                                                  */
/* ========================================================================== */

/*
 * Test basic template creation
 */
static void
test_resource_template_new (void)
{
    g_autoptr(McpResourceTemplate) tmpl = NULL;

    tmpl = mcp_resource_template_new ("file:///documents/{name}", "document-template");

    g_assert_nonnull (tmpl);
    g_assert_cmpstr (mcp_resource_template_get_uri_template (tmpl), ==, "file:///documents/{name}");
    g_assert_cmpstr (mcp_resource_template_get_name (tmpl), ==, "document-template");
    g_assert_null (mcp_resource_template_get_title (tmpl));
    g_assert_null (mcp_resource_template_get_description (tmpl));
    g_assert_null (mcp_resource_template_get_mime_type (tmpl));
}

/*
 * Test template properties
 */
static void
test_resource_template_properties (void)
{
    g_autoptr(McpResourceTemplate) tmpl = NULL;

    tmpl = mcp_resource_template_new ("db://users/{id}", "user-template");

    /* Test title property */
    g_assert_null (mcp_resource_template_get_title (tmpl));
    mcp_resource_template_set_title (tmpl, "User Profile");
    g_assert_cmpstr (mcp_resource_template_get_title (tmpl), ==, "User Profile");

    /* Test description property */
    mcp_resource_template_set_description (tmpl, "Access user profile by ID");
    g_assert_cmpstr (mcp_resource_template_get_description (tmpl), ==, "Access user profile by ID");

    /* Test MIME type property */
    mcp_resource_template_set_mime_type (tmpl, "application/json");
    g_assert_cmpstr (mcp_resource_template_get_mime_type (tmpl), ==, "application/json");

    /* Test changing uri_template */
    mcp_resource_template_set_uri_template (tmpl, "db://profiles/{id}");
    g_assert_cmpstr (mcp_resource_template_get_uri_template (tmpl), ==, "db://profiles/{id}");

    /* Test changing name */
    mcp_resource_template_set_name (tmpl, "profile-template");
    g_assert_cmpstr (mcp_resource_template_get_name (tmpl), ==, "profile-template");
}

/*
 * Test template JSON serialization
 */
static void
test_resource_template_to_json (void)
{
    g_autoptr(McpResourceTemplate) tmpl = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    tmpl = mcp_resource_template_new ("logs://{date}/{service}", "log-template");
    mcp_resource_template_set_title (tmpl, "Service Logs");
    mcp_resource_template_set_description (tmpl, "Access logs by date and service");
    mcp_resource_template_set_mime_type (tmpl, "text/plain");

    json = mcp_resource_template_to_json (tmpl);

    g_assert_nonnull (json);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (json));

    obj = json_node_get_object (json);

    g_assert_true (json_object_has_member (obj, "uriTemplate"));
    g_assert_cmpstr (json_object_get_string_member (obj, "uriTemplate"), ==, "logs://{date}/{service}");

    g_assert_true (json_object_has_member (obj, "name"));
    g_assert_cmpstr (json_object_get_string_member (obj, "name"), ==, "log-template");

    g_assert_true (json_object_has_member (obj, "title"));
    g_assert_cmpstr (json_object_get_string_member (obj, "title"), ==, "Service Logs");

    g_assert_true (json_object_has_member (obj, "description"));
    g_assert_cmpstr (json_object_get_string_member (obj, "description"), ==, "Access logs by date and service");

    g_assert_true (json_object_has_member (obj, "mimeType"));
    g_assert_cmpstr (json_object_get_string_member (obj, "mimeType"), ==, "text/plain");
}

/*
 * Test template JSON deserialization
 */
static void
test_resource_template_from_json (void)
{
    g_autoptr(McpResourceTemplate) tmpl = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str =
        "{"
        "  \"uriTemplate\": \"cache://{key}\","
        "  \"name\": \"cache-template\","
        "  \"title\": \"Cache Entry\","
        "  \"description\": \"Access cache by key\","
        "  \"mimeType\": \"application/octet-stream\""
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    tmpl = mcp_resource_template_new_from_json (root, &error);

    g_assert_no_error (error);
    g_assert_nonnull (tmpl);

    g_assert_cmpstr (mcp_resource_template_get_uri_template (tmpl), ==, "cache://{key}");
    g_assert_cmpstr (mcp_resource_template_get_name (tmpl), ==, "cache-template");
    g_assert_cmpstr (mcp_resource_template_get_title (tmpl), ==, "Cache Entry");
    g_assert_cmpstr (mcp_resource_template_get_description (tmpl), ==, "Access cache by key");
    g_assert_cmpstr (mcp_resource_template_get_mime_type (tmpl), ==, "application/octet-stream");
}

/*
 * Test template JSON deserialization with missing uriTemplate (should fail)
 */
static void
test_resource_template_from_json_missing_uri_template (void)
{
    g_autoptr(McpResourceTemplate) tmpl = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str = "{ \"name\": \"no-uri-template\" }";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    tmpl = mcp_resource_template_new_from_json (root, &error);

    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_null (tmpl);
}

/*
 * Test template round-trip JSON serialization
 */
static void
test_resource_template_json_roundtrip (void)
{
    g_autoptr(McpResourceTemplate) original = NULL;
    g_autoptr(McpResourceTemplate) restored = NULL;
    g_autoptr(JsonNode) json = NULL;
    g_autoptr(GError) error = NULL;

    original = mcp_resource_template_new ("metrics://{period}/{metric}", "metrics-template");
    mcp_resource_template_set_title (original, "System Metrics");
    mcp_resource_template_set_description (original, "Access system metrics by period and type");
    mcp_resource_template_set_mime_type (original, "application/json");

    json = mcp_resource_template_to_json (original);
    restored = mcp_resource_template_new_from_json (json, &error);

    g_assert_no_error (error);
    g_assert_nonnull (restored);

    g_assert_cmpstr (mcp_resource_template_get_uri_template (original), ==,
                     mcp_resource_template_get_uri_template (restored));
    g_assert_cmpstr (mcp_resource_template_get_name (original), ==,
                     mcp_resource_template_get_name (restored));
    g_assert_cmpstr (mcp_resource_template_get_title (original), ==,
                     mcp_resource_template_get_title (restored));
    g_assert_cmpstr (mcp_resource_template_get_description (original), ==,
                     mcp_resource_template_get_description (restored));
    g_assert_cmpstr (mcp_resource_template_get_mime_type (original), ==,
                     mcp_resource_template_get_mime_type (restored));
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* McpResource tests */
    g_test_add_func ("/mcp/resource/new", test_resource_new);
    g_test_add_func ("/mcp/resource/properties", test_resource_properties);
    g_test_add_func ("/mcp/resource/to-json", test_resource_to_json);
    g_test_add_func ("/mcp/resource/to-json-minimal", test_resource_to_json_minimal);
    g_test_add_func ("/mcp/resource/from-json", test_resource_from_json);
    g_test_add_func ("/mcp/resource/from-json-missing-uri", test_resource_from_json_missing_uri);
    g_test_add_func ("/mcp/resource/from-json-missing-name", test_resource_from_json_missing_name);
    g_test_add_func ("/mcp/resource/json-roundtrip", test_resource_json_roundtrip);

    /* McpResourceTemplate tests */
    g_test_add_func ("/mcp/resource-template/new", test_resource_template_new);
    g_test_add_func ("/mcp/resource-template/properties", test_resource_template_properties);
    g_test_add_func ("/mcp/resource-template/to-json", test_resource_template_to_json);
    g_test_add_func ("/mcp/resource-template/from-json", test_resource_template_from_json);
    g_test_add_func ("/mcp/resource-template/from-json-missing-uri-template",
                     test_resource_template_from_json_missing_uri_template);
    g_test_add_func ("/mcp/resource-template/json-roundtrip", test_resource_template_json_roundtrip);

    return g_test_run ();
}
