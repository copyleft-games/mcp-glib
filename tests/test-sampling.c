/*
 * test-sampling.c - Unit tests for sampling types
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include <mcp/mcp.h>

/* McpSamplingMessage tests */

static void
test_sampling_message_new (void)
{
    g_autoptr(McpSamplingMessage) msg = NULL;

    msg = mcp_sampling_message_new (MCP_ROLE_USER);
    g_assert_nonnull (msg);
    g_assert_cmpint (mcp_sampling_message_get_role (msg), ==, MCP_ROLE_USER);
}

static void
test_sampling_message_add_text (void)
{
    g_autoptr(McpSamplingMessage) msg = NULL;
    JsonArray *content;

    msg = mcp_sampling_message_new (MCP_ROLE_ASSISTANT);
    mcp_sampling_message_add_text (msg, "Hello, world!");

    content = mcp_sampling_message_get_content (msg);
    g_assert_nonnull (content);
    g_assert_cmpuint (json_array_get_length (content), ==, 1);
}

static void
test_sampling_message_to_json (void)
{
    g_autoptr(McpSamplingMessage) msg = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;

    msg = mcp_sampling_message_new (MCP_ROLE_USER);
    mcp_sampling_message_add_text (msg, "Test message");

    node = mcp_sampling_message_to_json (msg);
    g_assert_nonnull (node);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (node));

    obj = json_node_get_object (node);
    g_assert_true (json_object_has_member (obj, "role"));
    g_assert_true (json_object_has_member (obj, "content"));
}

static void
test_sampling_message_from_json (void)
{
    g_autoptr(McpSamplingMessage) msg = NULL;
    g_autoptr(McpSamplingMessage) parsed = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(GError) error = NULL;

    msg = mcp_sampling_message_new (MCP_ROLE_ASSISTANT);
    mcp_sampling_message_add_text (msg, "Response text");

    node = mcp_sampling_message_to_json (msg);
    parsed = mcp_sampling_message_new_from_json (node, &error);

    g_assert_no_error (error);
    g_assert_nonnull (parsed);
    g_assert_cmpint (mcp_sampling_message_get_role (parsed), ==, MCP_ROLE_ASSISTANT);
}

/* McpSamplingResult tests */

static void
test_sampling_result_new (void)
{
    g_autoptr(McpSamplingResult) result = NULL;

    result = mcp_sampling_result_new (MCP_ROLE_ASSISTANT);
    g_assert_nonnull (result);
    g_assert_cmpint (mcp_sampling_result_get_role (result), ==, MCP_ROLE_ASSISTANT);
}

static void
test_sampling_result_text (void)
{
    g_autoptr(McpSamplingResult) result = NULL;

    result = mcp_sampling_result_new (MCP_ROLE_ASSISTANT);
    mcp_sampling_result_set_text (result, "Generated text");
    g_assert_cmpstr (mcp_sampling_result_get_text (result), ==, "Generated text");
}

static void
test_sampling_result_model (void)
{
    g_autoptr(McpSamplingResult) result = NULL;

    result = mcp_sampling_result_new (MCP_ROLE_ASSISTANT);
    g_assert_null (mcp_sampling_result_get_model (result));

    mcp_sampling_result_set_model (result, "gpt-4");
    g_assert_cmpstr (mcp_sampling_result_get_model (result), ==, "gpt-4");
}

static void
test_sampling_result_stop_reason (void)
{
    g_autoptr(McpSamplingResult) result = NULL;

    result = mcp_sampling_result_new (MCP_ROLE_ASSISTANT);
    g_assert_null (mcp_sampling_result_get_stop_reason (result));

    mcp_sampling_result_set_stop_reason (result, "endTurn");
    g_assert_cmpstr (mcp_sampling_result_get_stop_reason (result), ==, "endTurn");
}

static void
test_sampling_result_to_json (void)
{
    g_autoptr(McpSamplingResult) result = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;

    result = mcp_sampling_result_new (MCP_ROLE_ASSISTANT);
    mcp_sampling_result_set_text (result, "Generated text");
    mcp_sampling_result_set_model (result, "claude-3");
    mcp_sampling_result_set_stop_reason (result, "endTurn");

    node = mcp_sampling_result_to_json (result);
    g_assert_nonnull (node);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (node));

    obj = json_node_get_object (node);
    g_assert_true (json_object_has_member (obj, "role"));
    g_assert_true (json_object_has_member (obj, "content"));
    g_assert_true (json_object_has_member (obj, "model"));
    g_assert_true (json_object_has_member (obj, "stopReason"));
}

static void
test_sampling_result_roundtrip (void)
{
    g_autoptr(McpSamplingResult) result = NULL;
    g_autoptr(McpSamplingResult) parsed = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(GError) error = NULL;

    result = mcp_sampling_result_new (MCP_ROLE_ASSISTANT);
    mcp_sampling_result_set_text (result, "Test output");
    mcp_sampling_result_set_model (result, "test-model");
    mcp_sampling_result_set_stop_reason (result, "maxTokens");

    node = mcp_sampling_result_to_json (result);
    parsed = mcp_sampling_result_new_from_json (node, &error);

    g_assert_no_error (error);
    g_assert_nonnull (parsed);
    g_assert_cmpint (mcp_sampling_result_get_role (parsed), ==, MCP_ROLE_ASSISTANT);
    g_assert_cmpstr (mcp_sampling_result_get_model (parsed), ==, "test-model");
    g_assert_cmpstr (mcp_sampling_result_get_stop_reason (parsed), ==, "maxTokens");
}

/* McpModelPreferences tests */

static void
test_model_preferences_new (void)
{
    g_autoptr(McpModelPreferences) prefs = NULL;

    prefs = mcp_model_preferences_new ();
    g_assert_nonnull (prefs);
}

static void
test_model_preferences_hints (void)
{
    g_autoptr(McpModelPreferences) prefs = NULL;
    GList *hints;

    prefs = mcp_model_preferences_new ();
    g_assert_null (mcp_model_preferences_get_hints (prefs));

    mcp_model_preferences_add_hint (prefs, "claude-3-opus");
    mcp_model_preferences_add_hint (prefs, "gpt-4");

    hints = mcp_model_preferences_get_hints (prefs);
    g_assert_cmpuint (g_list_length (hints), ==, 2);
}

static void
test_model_preferences_priorities (void)
{
    g_autoptr(McpModelPreferences) prefs = NULL;

    prefs = mcp_model_preferences_new ();

    mcp_model_preferences_set_cost_priority (prefs, 0.5);
    g_assert_cmpfloat (mcp_model_preferences_get_cost_priority (prefs), ==, 0.5);

    mcp_model_preferences_set_speed_priority (prefs, 0.8);
    g_assert_cmpfloat (mcp_model_preferences_get_speed_priority (prefs), ==, 0.8);

    mcp_model_preferences_set_intelligence_priority (prefs, 1.0);
    g_assert_cmpfloat (mcp_model_preferences_get_intelligence_priority (prefs), ==, 1.0);
}

static void
test_model_preferences_to_json (void)
{
    g_autoptr(McpModelPreferences) prefs = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;

    prefs = mcp_model_preferences_new ();
    mcp_model_preferences_add_hint (prefs, "test-model");
    mcp_model_preferences_set_cost_priority (prefs, 0.3);

    node = mcp_model_preferences_to_json (prefs);
    g_assert_nonnull (node);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (node));

    obj = json_node_get_object (node);
    g_assert_true (json_object_has_member (obj, "hints"));
    g_assert_true (json_object_has_member (obj, "costPriority"));
}

static void
test_model_preferences_roundtrip (void)
{
    g_autoptr(McpModelPreferences) prefs = NULL;
    g_autoptr(McpModelPreferences) parsed = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(GError) error = NULL;

    prefs = mcp_model_preferences_new ();
    mcp_model_preferences_add_hint (prefs, "model-a");
    mcp_model_preferences_set_cost_priority (prefs, 0.5);
    mcp_model_preferences_set_speed_priority (prefs, 0.7);

    node = mcp_model_preferences_to_json (prefs);
    parsed = mcp_model_preferences_new_from_json (node, &error);

    g_assert_no_error (error);
    g_assert_nonnull (parsed);
    g_assert_cmpfloat (mcp_model_preferences_get_cost_priority (parsed), ==, 0.5);
    g_assert_cmpfloat (mcp_model_preferences_get_speed_priority (parsed), ==, 0.7);
}

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* McpSamplingMessage tests */
    g_test_add_func ("/mcp/sampling/message/new", test_sampling_message_new);
    g_test_add_func ("/mcp/sampling/message/add-text", test_sampling_message_add_text);
    g_test_add_func ("/mcp/sampling/message/to-json", test_sampling_message_to_json);
    g_test_add_func ("/mcp/sampling/message/from-json", test_sampling_message_from_json);

    /* McpSamplingResult tests */
    g_test_add_func ("/mcp/sampling/result/new", test_sampling_result_new);
    g_test_add_func ("/mcp/sampling/result/text", test_sampling_result_text);
    g_test_add_func ("/mcp/sampling/result/model", test_sampling_result_model);
    g_test_add_func ("/mcp/sampling/result/stop-reason", test_sampling_result_stop_reason);
    g_test_add_func ("/mcp/sampling/result/to-json", test_sampling_result_to_json);
    g_test_add_func ("/mcp/sampling/result/roundtrip", test_sampling_result_roundtrip);

    /* McpModelPreferences tests */
    g_test_add_func ("/mcp/sampling/preferences/new", test_model_preferences_new);
    g_test_add_func ("/mcp/sampling/preferences/hints", test_model_preferences_hints);
    g_test_add_func ("/mcp/sampling/preferences/priorities", test_model_preferences_priorities);
    g_test_add_func ("/mcp/sampling/preferences/to-json", test_model_preferences_to_json);
    g_test_add_func ("/mcp/sampling/preferences/roundtrip", test_model_preferences_roundtrip);

    return g_test_run ();
}
