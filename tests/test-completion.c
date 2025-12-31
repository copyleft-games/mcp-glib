/*
 * test-completion.c - Unit tests for McpCompletionResult type
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include <mcp/mcp.h>

static void
test_completion_new (void)
{
    g_autoptr(McpCompletionResult) result = NULL;

    result = mcp_completion_result_new ();
    g_assert_nonnull (result);
    g_assert_null (mcp_completion_result_get_values (result));
    g_assert_cmpint (mcp_completion_result_get_total (result), ==, -1);
    g_assert_false (mcp_completion_result_get_has_more (result));
}

static void
test_completion_add_values (void)
{
    g_autoptr(McpCompletionResult) result = NULL;
    GList *values;

    result = mcp_completion_result_new ();
    mcp_completion_result_add_value (result, "apple");
    mcp_completion_result_add_value (result, "apricot");
    mcp_completion_result_add_value (result, "avocado");

    values = mcp_completion_result_get_values (result);
    g_assert_cmpuint (g_list_length (values), ==, 3);
    g_assert_cmpstr (values->data, ==, "apple");
    g_assert_cmpstr (values->next->data, ==, "apricot");
    g_assert_cmpstr (values->next->next->data, ==, "avocado");
}

static void
test_completion_total (void)
{
    g_autoptr(McpCompletionResult) result = NULL;

    result = mcp_completion_result_new ();
    g_assert_cmpint (mcp_completion_result_get_total (result), ==, -1);

    mcp_completion_result_set_total (result, 100);
    g_assert_cmpint (mcp_completion_result_get_total (result), ==, 100);

    mcp_completion_result_set_total (result, 0);
    g_assert_cmpint (mcp_completion_result_get_total (result), ==, 0);
}

static void
test_completion_has_more (void)
{
    g_autoptr(McpCompletionResult) result = NULL;

    result = mcp_completion_result_new ();
    g_assert_false (mcp_completion_result_get_has_more (result));

    mcp_completion_result_set_has_more (result, TRUE);
    g_assert_true (mcp_completion_result_get_has_more (result));

    mcp_completion_result_set_has_more (result, FALSE);
    g_assert_false (mcp_completion_result_get_has_more (result));
}

static void
test_completion_to_json (void)
{
    g_autoptr(McpCompletionResult) result = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;
    JsonObject *completion;
    JsonArray *values;

    result = mcp_completion_result_new ();
    mcp_completion_result_add_value (result, "foo");
    mcp_completion_result_add_value (result, "bar");
    mcp_completion_result_set_total (result, 10);
    mcp_completion_result_set_has_more (result, TRUE);

    node = mcp_completion_result_to_json (result);
    g_assert_nonnull (node);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (node));

    obj = json_node_get_object (node);
    g_assert_true (json_object_has_member (obj, "completion"));

    completion = json_object_get_object_member (obj, "completion");
    g_assert_nonnull (completion);

    g_assert_true (json_object_has_member (completion, "values"));
    values = json_object_get_array_member (completion, "values");
    g_assert_cmpuint (json_array_get_length (values), ==, 2);

    g_assert_true (json_object_has_member (completion, "total"));
    g_assert_cmpint (json_object_get_int_member (completion, "total"), ==, 10);

    g_assert_true (json_object_has_member (completion, "hasMore"));
    g_assert_true (json_object_get_boolean_member (completion, "hasMore"));
}

static void
test_completion_to_json_minimal (void)
{
    g_autoptr(McpCompletionResult) result = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;
    JsonObject *completion;

    result = mcp_completion_result_new ();
    mcp_completion_result_add_value (result, "test");

    node = mcp_completion_result_to_json (result);
    obj = json_node_get_object (node);
    completion = json_object_get_object_member (obj, "completion");

    /* total is -1 so should not be included */
    g_assert_false (json_object_has_member (completion, "total"));
    /* has_more is false so should not be included */
    g_assert_false (json_object_has_member (completion, "hasMore"));
}

static void
test_completion_from_json (void)
{
    g_autoptr(McpCompletionResult) result = NULL;
    g_autoptr(McpCompletionResult) parsed = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(GError) error = NULL;
    GList *values;

    result = mcp_completion_result_new ();
    mcp_completion_result_add_value (result, "one");
    mcp_completion_result_add_value (result, "two");
    mcp_completion_result_set_total (result, 50);
    mcp_completion_result_set_has_more (result, TRUE);

    node = mcp_completion_result_to_json (result);
    parsed = mcp_completion_result_new_from_json (node, &error);

    g_assert_no_error (error);
    g_assert_nonnull (parsed);

    values = mcp_completion_result_get_values (parsed);
    g_assert_cmpuint (g_list_length (values), ==, 2);
    g_assert_cmpint (mcp_completion_result_get_total (parsed), ==, 50);
    g_assert_true (mcp_completion_result_get_has_more (parsed));
}

static void
test_completion_from_json_missing_completion (void)
{
    g_autoptr(McpCompletionResult) parsed = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(GError) error = NULL;
    JsonBuilder *builder;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "other");
    json_builder_add_string_value (builder, "data");
    json_builder_end_object (builder);

    node = json_builder_get_root (builder);
    g_object_unref (builder);

    parsed = mcp_completion_result_new_from_json (node, &error);

    g_assert_null (parsed);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
}

static void
test_completion_from_json_missing_values (void)
{
    g_autoptr(McpCompletionResult) parsed = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(GError) error = NULL;
    JsonBuilder *builder;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "completion");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "total");
    json_builder_add_int_value (builder, 5);
    json_builder_end_object (builder);
    json_builder_end_object (builder);

    node = json_builder_get_root (builder);
    g_object_unref (builder);

    parsed = mcp_completion_result_new_from_json (node, &error);

    g_assert_null (parsed);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
}

static void
test_completion_roundtrip (void)
{
    g_autoptr(McpCompletionResult) result = NULL;
    g_autoptr(McpCompletionResult) parsed = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(GError) error = NULL;

    result = mcp_completion_result_new ();
    mcp_completion_result_add_value (result, "alpha");
    mcp_completion_result_add_value (result, "beta");
    mcp_completion_result_add_value (result, "gamma");
    mcp_completion_result_set_total (result, 26);
    mcp_completion_result_set_has_more (result, TRUE);

    node = mcp_completion_result_to_json (result);
    parsed = mcp_completion_result_new_from_json (node, &error);

    g_assert_no_error (error);
    g_assert_nonnull (parsed);
    g_assert_cmpint (mcp_completion_result_get_total (parsed), ==, 26);
    g_assert_true (mcp_completion_result_get_has_more (parsed));

    GList *values = mcp_completion_result_get_values (parsed);
    g_assert_cmpuint (g_list_length (values), ==, 3);
}

static void
test_completion_refcount (void)
{
    McpCompletionResult *result;
    McpCompletionResult *ref;

    result = mcp_completion_result_new ();
    ref = mcp_completion_result_ref (result);

    g_assert_true (result == ref);

    mcp_completion_result_unref (ref);
    mcp_completion_result_unref (result);
}

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/mcp/completion/new", test_completion_new);
    g_test_add_func ("/mcp/completion/add-values", test_completion_add_values);
    g_test_add_func ("/mcp/completion/total", test_completion_total);
    g_test_add_func ("/mcp/completion/has-more", test_completion_has_more);
    g_test_add_func ("/mcp/completion/to-json", test_completion_to_json);
    g_test_add_func ("/mcp/completion/to-json-minimal", test_completion_to_json_minimal);
    g_test_add_func ("/mcp/completion/from-json", test_completion_from_json);
    g_test_add_func ("/mcp/completion/from-json-missing-completion", test_completion_from_json_missing_completion);
    g_test_add_func ("/mcp/completion/from-json-missing-values", test_completion_from_json_missing_values);
    g_test_add_func ("/mcp/completion/roundtrip", test_completion_roundtrip);
    g_test_add_func ("/mcp/completion/refcount", test_completion_refcount);

    return g_test_run ();
}
