/*
 * test-root.c - Unit tests for McpRoot type
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include "mcp.h"

static void
test_root_new (void)
{
    g_autoptr(McpRoot) root = NULL;

    root = mcp_root_new ("file:///home/user/project");
    g_assert_nonnull (root);
    g_assert_cmpstr (mcp_root_get_uri (root), ==, "file:///home/user/project");
    g_assert_null (mcp_root_get_name (root));
}

static void
test_root_with_name (void)
{
    g_autoptr(McpRoot) root = NULL;

    root = mcp_root_new ("file:///tmp");
    mcp_root_set_name (root, "Temp Directory");
    g_assert_cmpstr (mcp_root_get_uri (root), ==, "file:///tmp");
    g_assert_cmpstr (mcp_root_get_name (root), ==, "Temp Directory");
}

static void
test_root_set_uri (void)
{
    g_autoptr(McpRoot) root = NULL;

    root = mcp_root_new ("file:///old");
    mcp_root_set_uri (root, "file:///new");
    g_assert_cmpstr (mcp_root_get_uri (root), ==, "file:///new");
}

static void
test_root_to_json (void)
{
    g_autoptr(McpRoot) root = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;

    root = mcp_root_new ("file:///workspace");
    mcp_root_set_name (root, "Workspace");
    node = mcp_root_to_json (root);

    g_assert_nonnull (node);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (node));

    obj = json_node_get_object (node);
    g_assert_true (json_object_has_member (obj, "uri"));
    g_assert_cmpstr (json_object_get_string_member (obj, "uri"), ==, "file:///workspace");
    g_assert_true (json_object_has_member (obj, "name"));
    g_assert_cmpstr (json_object_get_string_member (obj, "name"), ==, "Workspace");
}

static void
test_root_to_json_no_name (void)
{
    g_autoptr(McpRoot) root = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;

    root = mcp_root_new ("file:///data");
    node = mcp_root_to_json (root);

    g_assert_nonnull (node);
    obj = json_node_get_object (node);
    g_assert_true (json_object_has_member (obj, "uri"));
    g_assert_false (json_object_has_member (obj, "name"));
}

static void
test_root_from_json (void)
{
    g_autoptr(McpRoot) root = NULL;
    g_autoptr(McpRoot) parsed = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(GError) error = NULL;

    root = mcp_root_new ("file:///home/user");
    mcp_root_set_name (root, "Home");
    node = mcp_root_to_json (root);

    parsed = mcp_root_new_from_json (node, &error);

    g_assert_no_error (error);
    g_assert_nonnull (parsed);
    g_assert_cmpstr (mcp_root_get_uri (parsed), ==, "file:///home/user");
    g_assert_cmpstr (mcp_root_get_name (parsed), ==, "Home");
}

static void
test_root_from_json_missing_uri (void)
{
    g_autoptr(McpRoot) parsed = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(GError) error = NULL;
    JsonBuilder *builder;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "name");
    json_builder_add_string_value (builder, "Test");
    json_builder_end_object (builder);

    node = json_builder_get_root (builder);
    g_object_unref (builder);

    parsed = mcp_root_new_from_json (node, &error);

    g_assert_null (parsed);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
}

static void
test_root_roundtrip (void)
{
    g_autoptr(McpRoot) root = NULL;
    g_autoptr(McpRoot) parsed = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(GError) error = NULL;

    root = mcp_root_new ("file:///var/www");
    mcp_root_set_name (root, "Web Root");
    node = mcp_root_to_json (root);
    parsed = mcp_root_new_from_json (node, &error);

    g_assert_no_error (error);
    g_assert_nonnull (parsed);
    g_assert_cmpstr (mcp_root_get_uri (parsed), ==, mcp_root_get_uri (root));
    g_assert_cmpstr (mcp_root_get_name (parsed), ==, mcp_root_get_name (root));
}

static void
test_root_refcount (void)
{
    McpRoot *root;
    McpRoot *ref;

    root = mcp_root_new ("file:///test");
    ref = mcp_root_ref (root);

    g_assert_true (root == ref);

    mcp_root_unref (ref);
    mcp_root_unref (root);
}

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/mcp/root/new", test_root_new);
    g_test_add_func ("/mcp/root/with-name", test_root_with_name);
    g_test_add_func ("/mcp/root/set-uri", test_root_set_uri);
    g_test_add_func ("/mcp/root/to-json", test_root_to_json);
    g_test_add_func ("/mcp/root/to-json-no-name", test_root_to_json_no_name);
    g_test_add_func ("/mcp/root/from-json", test_root_from_json);
    g_test_add_func ("/mcp/root/from-json-missing-uri", test_root_from_json_missing_uri);
    g_test_add_func ("/mcp/root/roundtrip", test_root_roundtrip);
    g_test_add_func ("/mcp/root/refcount", test_root_refcount);

    return g_test_run ();
}
