/*
 * test-tool.c - Unit tests for McpTool
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include <mcp/mcp.h>

/*
 * Test basic tool creation
 */
static void
test_tool_new (void)
{
    g_autoptr(McpTool) tool = NULL;

    tool = mcp_tool_new ("test-tool", "A test tool");

    g_assert_nonnull (tool);
    g_assert_cmpstr (mcp_tool_get_name (tool), ==, "test-tool");
    g_assert_cmpstr (mcp_tool_get_description (tool), ==, "A test tool");
    g_assert_null (mcp_tool_get_title (tool));
    g_assert_null (mcp_tool_get_input_schema (tool));
    g_assert_null (mcp_tool_get_output_schema (tool));
}

/*
 * Test tool with NULL description
 */
static void
test_tool_new_null_description (void)
{
    g_autoptr(McpTool) tool = NULL;

    tool = mcp_tool_new ("null-desc", NULL);

    g_assert_nonnull (tool);
    g_assert_cmpstr (mcp_tool_get_name (tool), ==, "null-desc");
    g_assert_null (mcp_tool_get_description (tool));
}

/*
 * Test tool properties
 */
static void
test_tool_properties (void)
{
    g_autoptr(McpTool) tool = NULL;

    tool = mcp_tool_new ("prop-test", "Description");

    /* Test title property */
    g_assert_null (mcp_tool_get_title (tool));
    mcp_tool_set_title (tool, "Test Title");
    g_assert_cmpstr (mcp_tool_get_title (tool), ==, "Test Title");

    /* Test changing name */
    mcp_tool_set_name (tool, "new-name");
    g_assert_cmpstr (mcp_tool_get_name (tool), ==, "new-name");

    /* Test changing description */
    mcp_tool_set_description (tool, "New description");
    g_assert_cmpstr (mcp_tool_get_description (tool), ==, "New description");
}

/*
 * Test annotation hints defaults
 */
static void
test_tool_hints_defaults (void)
{
    g_autoptr(McpTool) tool = NULL;

    tool = mcp_tool_new ("hints-test", NULL);

    /* Verify defaults */
    g_assert_false (mcp_tool_get_read_only_hint (tool));
    g_assert_true (mcp_tool_get_destructive_hint (tool));
    g_assert_false (mcp_tool_get_idempotent_hint (tool));
    g_assert_true (mcp_tool_get_open_world_hint (tool));
}

/*
 * Test setting annotation hints
 */
static void
test_tool_hints_setters (void)
{
    g_autoptr(McpTool) tool = NULL;

    tool = mcp_tool_new ("hints-test", NULL);

    mcp_tool_set_read_only_hint (tool, TRUE);
    g_assert_true (mcp_tool_get_read_only_hint (tool));

    mcp_tool_set_destructive_hint (tool, FALSE);
    g_assert_false (mcp_tool_get_destructive_hint (tool));

    mcp_tool_set_idempotent_hint (tool, TRUE);
    g_assert_true (mcp_tool_get_idempotent_hint (tool));

    mcp_tool_set_open_world_hint (tool, FALSE);
    g_assert_false (mcp_tool_get_open_world_hint (tool));
}

/*
 * Test input schema
 */
static void
test_tool_input_schema (void)
{
    g_autoptr(McpTool) tool = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) schema = NULL;
    JsonNode *retrieved;

    tool = mcp_tool_new ("schema-test", NULL);

    /* Build a simple schema */
    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "object");
    json_builder_set_member_name (builder, "properties");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "name");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "string");
    json_builder_end_object (builder);
    json_builder_end_object (builder);
    json_builder_end_object (builder);

    schema = json_builder_get_root (builder);

    mcp_tool_set_input_schema (tool, schema);

    retrieved = mcp_tool_get_input_schema (tool);
    g_assert_nonnull (retrieved);

    /* Verify the schema content */
    g_assert_true (JSON_NODE_HOLDS_OBJECT (retrieved));
}

/*
 * Test JSON serialization
 */
static void
test_tool_to_json (void)
{
    g_autoptr(McpTool) tool = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    tool = mcp_tool_new ("json-test", "A tool for JSON testing");
    mcp_tool_set_title (tool, "JSON Test Tool");

    json = mcp_tool_to_json (tool);

    g_assert_nonnull (json);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (json));

    obj = json_node_get_object (json);

    g_assert_true (json_object_has_member (obj, "name"));
    g_assert_cmpstr (json_object_get_string_member (obj, "name"), ==, "json-test");

    g_assert_true (json_object_has_member (obj, "description"));
    g_assert_cmpstr (json_object_get_string_member (obj, "description"), ==, "A tool for JSON testing");

    g_assert_true (json_object_has_member (obj, "inputSchema"));

    /* Title should be in annotations */
    g_assert_true (json_object_has_member (obj, "annotations"));
}

/*
 * Test JSON deserialization
 */
static void
test_tool_from_json (void)
{
    g_autoptr(McpTool) tool = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str =
        "{"
        "  \"name\": \"from-json-tool\","
        "  \"description\": \"Created from JSON\","
        "  \"title\": \"From JSON\","
        "  \"inputSchema\": {"
        "    \"type\": \"object\","
        "    \"properties\": {"
        "      \"value\": { \"type\": \"number\" }"
        "    }"
        "  },"
        "  \"annotations\": {"
        "    \"readOnlyHint\": true,"
        "    \"idempotentHint\": true"
        "  }"
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    tool = mcp_tool_new_from_json (root, &error);

    g_assert_no_error (error);
    g_assert_nonnull (tool);

    g_assert_cmpstr (mcp_tool_get_name (tool), ==, "from-json-tool");
    g_assert_cmpstr (mcp_tool_get_description (tool), ==, "Created from JSON");
    g_assert_cmpstr (mcp_tool_get_title (tool), ==, "From JSON");
    g_assert_nonnull (mcp_tool_get_input_schema (tool));
    g_assert_true (mcp_tool_get_read_only_hint (tool));
    g_assert_true (mcp_tool_get_idempotent_hint (tool));
}

/*
 * Test JSON deserialization with missing name (should fail)
 */
static void
test_tool_from_json_missing_name (void)
{
    g_autoptr(McpTool) tool = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str = "{ \"description\": \"No name\" }";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    tool = mcp_tool_new_from_json (root, &error);

    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_null (tool);
}

/*
 * Test round-trip JSON serialization
 */
static void
test_tool_json_roundtrip (void)
{
    g_autoptr(McpTool) original = NULL;
    g_autoptr(McpTool) restored = NULL;
    g_autoptr(JsonNode) json = NULL;
    g_autoptr(GError) error = NULL;

    original = mcp_tool_new ("roundtrip", "Round-trip test");
    mcp_tool_set_title (original, "Round Trip");
    mcp_tool_set_read_only_hint (original, TRUE);
    mcp_tool_set_destructive_hint (original, FALSE);

    json = mcp_tool_to_json (original);
    restored = mcp_tool_new_from_json (json, &error);

    g_assert_no_error (error);
    g_assert_nonnull (restored);

    g_assert_cmpstr (mcp_tool_get_name (original), ==, mcp_tool_get_name (restored));
    g_assert_cmpstr (mcp_tool_get_description (original), ==, mcp_tool_get_description (restored));
    g_assert_cmpstr (mcp_tool_get_title (original), ==, mcp_tool_get_title (restored));
    g_assert_cmpint (mcp_tool_get_read_only_hint (original), ==, mcp_tool_get_read_only_hint (restored));
    g_assert_cmpint (mcp_tool_get_destructive_hint (original), ==, mcp_tool_get_destructive_hint (restored));
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/mcp/tool/new", test_tool_new);
    g_test_add_func ("/mcp/tool/new-null-description", test_tool_new_null_description);
    g_test_add_func ("/mcp/tool/properties", test_tool_properties);
    g_test_add_func ("/mcp/tool/hints/defaults", test_tool_hints_defaults);
    g_test_add_func ("/mcp/tool/hints/setters", test_tool_hints_setters);
    g_test_add_func ("/mcp/tool/input-schema", test_tool_input_schema);
    g_test_add_func ("/mcp/tool/to-json", test_tool_to_json);
    g_test_add_func ("/mcp/tool/from-json", test_tool_from_json);
    g_test_add_func ("/mcp/tool/from-json-missing-name", test_tool_from_json_missing_name);
    g_test_add_func ("/mcp/tool/json-roundtrip", test_tool_json_roundtrip);

    return g_test_run ();
}
