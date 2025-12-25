/*
 * test-task.c - Unit tests for McpTask
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include <mcp/mcp.h>

/*
 * Test basic task creation
 */
static void
test_task_new (void)
{
    g_autoptr(McpTask) task = NULL;
    GDateTime *created_at;
    GDateTime *updated_at;

    task = mcp_task_new ("task-123", MCP_TASK_STATUS_WORKING);

    g_assert_nonnull (task);
    g_assert_cmpstr (mcp_task_get_task_id (task), ==, "task-123");
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_WORKING);
    g_assert_null (mcp_task_get_status_message (task));

    /* Verify timestamps are set */
    created_at = mcp_task_get_created_at (task);
    g_assert_nonnull (created_at);

    updated_at = mcp_task_get_last_updated_at (task);
    g_assert_nonnull (updated_at);

    /* Verify defaults */
    g_assert_cmpint (mcp_task_get_ttl (task), ==, -1);
    g_assert_cmpint (mcp_task_get_poll_interval (task), ==, -1);
}

/*
 * Test task with different statuses
 */
static void
test_task_statuses (void)
{
    g_autoptr(McpTask) task = NULL;

    task = mcp_task_new ("task-456", MCP_TASK_STATUS_COMPLETED);
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_COMPLETED);

    mcp_task_set_status (task, MCP_TASK_STATUS_FAILED);
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_FAILED);

    mcp_task_set_status (task, MCP_TASK_STATUS_CANCELLED);
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_CANCELLED);

    mcp_task_set_status (task, MCP_TASK_STATUS_INPUT_REQUIRED);
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_INPUT_REQUIRED);
}

/*
 * Test task properties
 */
static void
test_task_properties (void)
{
    g_autoptr(McpTask) task = NULL;
    g_autoptr(GDateTime) new_time = NULL;

    task = mcp_task_new ("task-789", MCP_TASK_STATUS_WORKING);

    /* Test status message */
    g_assert_null (mcp_task_get_status_message (task));
    mcp_task_set_status_message (task, "Processing data...");
    g_assert_cmpstr (mcp_task_get_status_message (task), ==, "Processing data...");

    /* Test task ID change */
    mcp_task_set_task_id (task, "task-999");
    g_assert_cmpstr (mcp_task_get_task_id (task), ==, "task-999");

    /* Test TTL */
    g_assert_cmpint (mcp_task_get_ttl (task), ==, -1);
    mcp_task_set_ttl (task, 60000);
    g_assert_cmpint (mcp_task_get_ttl (task), ==, 60000);

    /* Test poll interval */
    g_assert_cmpint (mcp_task_get_poll_interval (task), ==, -1);
    mcp_task_set_poll_interval (task, 5000);
    g_assert_cmpint (mcp_task_get_poll_interval (task), ==, 5000);

    /* Test setting timestamps */
    new_time = g_date_time_new_utc (2025, 1, 1, 12, 0, 0);
    mcp_task_set_created_at (task, new_time);
    g_assert_nonnull (mcp_task_get_created_at (task));
}

/*
 * Test timestamp update
 */
static void
test_task_update_timestamp (void)
{
    g_autoptr(McpTask) task = NULL;
    GDateTime *before;
    GDateTime *after;

    task = mcp_task_new ("task-abc", MCP_TASK_STATUS_WORKING);

    before = mcp_task_get_last_updated_at (task);
    g_assert_nonnull (before);

    /* Wait a tiny bit and update */
    g_usleep (1000);
    mcp_task_update_timestamp (task);

    after = mcp_task_get_last_updated_at (task);
    g_assert_nonnull (after);

    /* After should be same or later than before */
    g_assert_cmpint (g_date_time_compare (after, before), >=, 0);
}

/*
 * Test JSON serialization
 */
static void
test_task_to_json (void)
{
    g_autoptr(McpTask) task = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    task = mcp_task_new ("task-json", MCP_TASK_STATUS_WORKING);
    mcp_task_set_status_message (task, "Processing...");
    mcp_task_set_ttl (task, 120000);
    mcp_task_set_poll_interval (task, 10000);

    json = mcp_task_to_json (task);

    g_assert_nonnull (json);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (json));

    obj = json_node_get_object (json);

    g_assert_true (json_object_has_member (obj, "taskId"));
    g_assert_cmpstr (json_object_get_string_member (obj, "taskId"), ==, "task-json");

    g_assert_true (json_object_has_member (obj, "status"));
    g_assert_cmpstr (json_object_get_string_member (obj, "status"), ==, "working");

    g_assert_true (json_object_has_member (obj, "statusMessage"));
    g_assert_cmpstr (json_object_get_string_member (obj, "statusMessage"), ==, "Processing...");

    g_assert_true (json_object_has_member (obj, "createdAt"));
    g_assert_true (json_object_has_member (obj, "lastUpdatedAt"));

    g_assert_true (json_object_has_member (obj, "ttl"));
    g_assert_cmpint (json_object_get_int_member (obj, "ttl"), ==, 120000);

    g_assert_true (json_object_has_member (obj, "pollInterval"));
    g_assert_cmpint (json_object_get_int_member (obj, "pollInterval"), ==, 10000);
}

/*
 * Test JSON serialization with unlimited TTL
 */
static void
test_task_to_json_unlimited_ttl (void)
{
    g_autoptr(McpTask) task = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;
    JsonNode *ttl_node;

    task = mcp_task_new ("task-unlimited", MCP_TASK_STATUS_COMPLETED);

    json = mcp_task_to_json (task);

    g_assert_nonnull (json);
    obj = json_node_get_object (json);

    /* TTL should be null for unlimited */
    g_assert_true (json_object_has_member (obj, "ttl"));
    ttl_node = json_object_get_member (obj, "ttl");
    g_assert_true (json_node_is_null (ttl_node));

    /* No poll interval if not set */
    g_assert_false (json_object_has_member (obj, "pollInterval"));
}

/*
 * Test JSON deserialization
 */
static void
test_task_from_json (void)
{
    g_autoptr(McpTask) task = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str =
        "{"
        "  \"taskId\": \"task-from-json\","
        "  \"status\": \"input_required\","
        "  \"statusMessage\": \"Need user confirmation\","
        "  \"createdAt\": \"2025-01-15T10:30:00Z\","
        "  \"lastUpdatedAt\": \"2025-01-15T10:35:00Z\","
        "  \"ttl\": 300000,"
        "  \"pollInterval\": 15000"
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    task = mcp_task_new_from_json (root, &error);

    g_assert_no_error (error);
    g_assert_nonnull (task);

    g_assert_cmpstr (mcp_task_get_task_id (task), ==, "task-from-json");
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_INPUT_REQUIRED);
    g_assert_cmpstr (mcp_task_get_status_message (task), ==, "Need user confirmation");
    g_assert_cmpint (mcp_task_get_ttl (task), ==, 300000);
    g_assert_cmpint (mcp_task_get_poll_interval (task), ==, 15000);

    /* Verify timestamps were parsed */
    GDateTime *created = mcp_task_get_created_at (task);
    g_assert_nonnull (created);
    g_assert_cmpint (g_date_time_get_year (created), ==, 2025);
    g_assert_cmpint (g_date_time_get_month (created), ==, 1);
    g_assert_cmpint (g_date_time_get_day_of_month (created), ==, 15);
}

/*
 * Test JSON deserialization with null TTL
 */
static void
test_task_from_json_null_ttl (void)
{
    g_autoptr(McpTask) task = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str =
        "{"
        "  \"taskId\": \"task-null-ttl\","
        "  \"status\": \"completed\","
        "  \"createdAt\": \"2025-01-15T10:30:00Z\","
        "  \"lastUpdatedAt\": \"2025-01-15T10:40:00Z\","
        "  \"ttl\": null"
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    task = mcp_task_new_from_json (root, &error);

    g_assert_no_error (error);
    g_assert_nonnull (task);

    g_assert_cmpint (mcp_task_get_ttl (task), ==, -1);
}

/*
 * Test JSON deserialization with missing taskId (should fail)
 */
static void
test_task_from_json_missing_task_id (void)
{
    g_autoptr(McpTask) task = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str =
        "{"
        "  \"status\": \"working\","
        "  \"createdAt\": \"2025-01-15T10:30:00Z\","
        "  \"lastUpdatedAt\": \"2025-01-15T10:35:00Z\""
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    task = mcp_task_new_from_json (root, &error);

    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_null (task);
}

/*
 * Test JSON deserialization with missing status (should fail)
 */
static void
test_task_from_json_missing_status (void)
{
    g_autoptr(McpTask) task = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str =
        "{"
        "  \"taskId\": \"task-no-status\","
        "  \"createdAt\": \"2025-01-15T10:30:00Z\","
        "  \"lastUpdatedAt\": \"2025-01-15T10:35:00Z\""
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    task = mcp_task_new_from_json (root, &error);

    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_null (task);
}

/*
 * Test round-trip JSON serialization
 */
static void
test_task_json_roundtrip (void)
{
    g_autoptr(McpTask) original = NULL;
    g_autoptr(McpTask) restored = NULL;
    g_autoptr(JsonNode) json = NULL;
    g_autoptr(GError) error = NULL;

    original = mcp_task_new ("task-roundtrip", MCP_TASK_STATUS_WORKING);
    mcp_task_set_status_message (original, "In progress");
    mcp_task_set_ttl (original, 60000);
    mcp_task_set_poll_interval (original, 5000);

    json = mcp_task_to_json (original);
    restored = mcp_task_new_from_json (json, &error);

    g_assert_no_error (error);
    g_assert_nonnull (restored);

    g_assert_cmpstr (mcp_task_get_task_id (original), ==, mcp_task_get_task_id (restored));
    g_assert_cmpint (mcp_task_get_status (original), ==, mcp_task_get_status (restored));
    g_assert_cmpstr (mcp_task_get_status_message (original), ==, mcp_task_get_status_message (restored));
    g_assert_cmpint (mcp_task_get_ttl (original), ==, mcp_task_get_ttl (restored));
    g_assert_cmpint (mcp_task_get_poll_interval (original), ==, mcp_task_get_poll_interval (restored));
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/mcp/task/new", test_task_new);
    g_test_add_func ("/mcp/task/statuses", test_task_statuses);
    g_test_add_func ("/mcp/task/properties", test_task_properties);
    g_test_add_func ("/mcp/task/update-timestamp", test_task_update_timestamp);
    g_test_add_func ("/mcp/task/to-json", test_task_to_json);
    g_test_add_func ("/mcp/task/to-json-unlimited-ttl", test_task_to_json_unlimited_ttl);
    g_test_add_func ("/mcp/task/from-json", test_task_from_json);
    g_test_add_func ("/mcp/task/from-json-null-ttl", test_task_from_json_null_ttl);
    g_test_add_func ("/mcp/task/from-json-missing-task-id", test_task_from_json_missing_task_id);
    g_test_add_func ("/mcp/task/from-json-missing-status", test_task_from_json_missing_status);
    g_test_add_func ("/mcp/task/json-roundtrip", test_task_json_roundtrip);

    return g_test_run ();
}
