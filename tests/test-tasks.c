/*
 * test-tasks.c - Tests for Tasks API (Experimental)
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include "mcp.h"

/* Test task creation */
static void
test_task_new (void)
{
    g_autoptr(McpTask) task = NULL;

    task = mcp_task_new ("test-task-1", MCP_TASK_STATUS_WORKING);
    g_assert_nonnull (task);
    g_assert_true (MCP_IS_TASK (task));
}

/* Test task ID */
static void
test_task_id (void)
{
    g_autoptr(McpTask) task = NULL;

    task = mcp_task_new ("my-task-id", MCP_TASK_STATUS_WORKING);
    g_assert_cmpstr (mcp_task_get_task_id (task), ==, "my-task-id");
}

/* Test task status */
static void
test_task_status (void)
{
    g_autoptr(McpTask) task = NULL;

    task = mcp_task_new ("test-task", MCP_TASK_STATUS_WORKING);
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_WORKING);

    mcp_task_set_status (task, MCP_TASK_STATUS_COMPLETED);
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_COMPLETED);

    mcp_task_set_status (task, MCP_TASK_STATUS_FAILED);
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_FAILED);

    mcp_task_set_status (task, MCP_TASK_STATUS_CANCELLED);
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_CANCELLED);

    mcp_task_set_status (task, MCP_TASK_STATUS_INPUT_REQUIRED);
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_INPUT_REQUIRED);
}

/* Test task status message */
static void
test_task_status_message (void)
{
    g_autoptr(McpTask) task = NULL;

    task = mcp_task_new ("test-task", MCP_TASK_STATUS_WORKING);

    g_assert_null (mcp_task_get_status_message (task));

    mcp_task_set_status_message (task, "Processing...");
    g_assert_cmpstr (mcp_task_get_status_message (task), ==, "Processing...");

    mcp_task_set_status_message (task, "Completed successfully");
    g_assert_cmpstr (mcp_task_get_status_message (task), ==, "Completed successfully");
}

/* Test task timestamps */
static void
test_task_timestamps (void)
{
    g_autoptr(McpTask) task = NULL;
    GDateTime *created, *updated;

    task = mcp_task_new ("test-task", MCP_TASK_STATUS_WORKING);

    created = mcp_task_get_created_at (task);
    g_assert_nonnull (created);

    updated = mcp_task_get_last_updated_at (task);
    g_assert_nonnull (updated);

    /* Wait a bit and update */
    g_usleep (10000); /* 10ms */
    mcp_task_update_timestamp (task);

    updated = mcp_task_get_last_updated_at (task);
    g_assert_nonnull (updated);
}

/* Test task TTL */
static void
test_task_ttl (void)
{
    g_autoptr(McpTask) task = NULL;

    task = mcp_task_new ("test-task", MCP_TASK_STATUS_WORKING);

    /* Default TTL is -1 (unlimited) */
    g_assert_cmpint (mcp_task_get_ttl (task), ==, -1);

    mcp_task_set_ttl (task, 3600);
    g_assert_cmpint (mcp_task_get_ttl (task), ==, 3600);
}

/* Test task poll interval */
static void
test_task_poll_interval (void)
{
    g_autoptr(McpTask) task = NULL;

    task = mcp_task_new ("test-task", MCP_TASK_STATUS_WORKING);

    /* Default poll interval is -1 (not set) */
    g_assert_cmpint (mcp_task_get_poll_interval (task), ==, -1);

    mcp_task_set_poll_interval (task, 5000);
    g_assert_cmpint (mcp_task_get_poll_interval (task), ==, 5000);
}

/* Test task JSON serialization */
static void
test_task_to_json (void)
{
    g_autoptr(McpTask) task = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;

    task = mcp_task_new ("task-123", MCP_TASK_STATUS_WORKING);
    mcp_task_set_status_message (task, "In progress");
    mcp_task_set_ttl (task, 7200);
    mcp_task_set_poll_interval (task, 10000);

    node = mcp_task_to_json (task);
    g_assert_nonnull (node);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (node));

    obj = json_node_get_object (node);
    g_assert_true (json_object_has_member (obj, "taskId"));
    g_assert_cmpstr (json_object_get_string_member (obj, "taskId"), ==, "task-123");

    g_assert_true (json_object_has_member (obj, "status"));
    g_assert_cmpstr (json_object_get_string_member (obj, "status"), ==, "working");

    g_assert_true (json_object_has_member (obj, "statusMessage"));
    g_assert_cmpstr (json_object_get_string_member (obj, "statusMessage"), ==, "In progress");
}

/* Test task JSON deserialization */
static void
test_task_from_json (void)
{
    g_autoptr(McpTask) task = NULL;
    g_autoptr(JsonParser) parser = NULL;
    JsonNode *root;
    GError *error = NULL;

    const gchar *json_str = "{"
        "\"taskId\": \"task-456\","
        "\"status\": \"completed\","
        "\"statusMessage\": \"Done\","
        "\"createdAt\": \"2025-01-01T00:00:00Z\","
        "\"lastUpdatedAt\": \"2025-01-01T00:01:00Z\","
        "\"ttl\": 1800,"
        "\"pollInterval\": 5000"
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    task = mcp_task_new_from_json (root, &error);

    g_assert_nonnull (task);
    g_assert_no_error (error);
    g_assert_cmpstr (mcp_task_get_task_id (task), ==, "task-456");
    g_assert_cmpint (mcp_task_get_status (task), ==, MCP_TASK_STATUS_COMPLETED);
    g_assert_cmpstr (mcp_task_get_status_message (task), ==, "Done");
    g_assert_cmpint (mcp_task_get_ttl (task), ==, 1800);
    g_assert_cmpint (mcp_task_get_poll_interval (task), ==, 5000);
}

/* Test server task management - add async tool */
static void
test_server_add_async_tool (void)
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpTool) tool = NULL;

    server = mcp_server_new ("test-server", "1.0.0");
    tool = mcp_tool_new ("async-tool", "An async tool");

    mcp_server_add_async_tool (server, tool, NULL, NULL, NULL);

    /* Tool should be listed */
    {
        GList *tools = mcp_server_list_tools (server);
        g_assert_cmpuint (g_list_length (tools), ==, 1);

        McpTool *found = tools->data;
        g_assert_cmpstr (mcp_tool_get_name (found), ==, "async-tool");

        g_list_free_full (tools, g_object_unref);
    }
}

/* Test server task list (empty) */
static void
test_server_list_tasks_empty (void)
{
    g_autoptr(McpServer) server = NULL;
    GList *tasks;

    server = mcp_server_new ("test-server", "1.0.0");
    tasks = mcp_server_list_tasks (server);

    g_assert_null (tasks);
}

/* Test server get task (not found) */
static void
test_server_get_task_not_found (void)
{
    g_autoptr(McpServer) server = NULL;
    McpTask *task;

    server = mcp_server_new ("test-server", "1.0.0");
    task = mcp_server_get_task (server, "nonexistent");

    g_assert_null (task);
}

/* Test server cancel task (not found) */
static void
test_server_cancel_task_not_found (void)
{
    g_autoptr(McpServer) server = NULL;
    gboolean result;

    server = mcp_server_new ("test-server", "1.0.0");
    result = mcp_server_cancel_task (server, "nonexistent");

    g_assert_false (result);
}

/* Test task properties via GObject */
static void
test_task_properties (void)
{
    g_autoptr(McpTask) task = NULL;
    g_autofree gchar *task_id = NULL;
    g_autofree gchar *status_message = NULL;
    McpTaskStatus status;
    gint64 ttl;
    gint64 poll_interval;

    task = mcp_task_new ("prop-test", MCP_TASK_STATUS_WORKING);
    mcp_task_set_status_message (task, "Testing");
    mcp_task_set_ttl (task, 600);
    mcp_task_set_poll_interval (task, 5000);

    g_object_get (task,
                  "task-id", &task_id,
                  "status", &status,
                  "status-message", &status_message,
                  "ttl", &ttl,
                  "poll-interval", &poll_interval,
                  NULL);

    g_assert_cmpstr (task_id, ==, "prop-test");
    g_assert_cmpint (status, ==, MCP_TASK_STATUS_WORKING);
    g_assert_cmpstr (status_message, ==, "Testing");
    g_assert_cmpint (ttl, ==, 600);
    g_assert_cmpint (poll_interval, ==, 5000);
}

/* Test update task status helper */
static void
test_server_update_task_status (void)
{
    g_autoptr(McpServer) server = NULL;
    gboolean result;

    server = mcp_server_new ("test-server", "1.0.0");

    /* Task doesn't exist */
    result = mcp_server_update_task_status (server, "nonexistent",
                                             MCP_TASK_STATUS_COMPLETED, "Done");
    g_assert_false (result);
}

/* Test complete task helper (task not found) */
static void
test_server_complete_task_not_found (void)
{
    g_autoptr(McpServer) server = NULL;
    gboolean result;

    server = mcp_server_new ("test-server", "1.0.0");

    result = mcp_server_complete_task (server, "nonexistent", NULL, NULL);
    g_assert_false (result);
}

/* Test fail task helper (task not found) */
static void
test_server_fail_task_not_found (void)
{
    g_autoptr(McpServer) server = NULL;
    gboolean result;

    server = mcp_server_new ("test-server", "1.0.0");

    result = mcp_server_fail_task (server, "nonexistent", "Error occurred");
    g_assert_false (result);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Task type tests */
    g_test_add_func ("/tasks/new", test_task_new);
    g_test_add_func ("/tasks/id", test_task_id);
    g_test_add_func ("/tasks/status", test_task_status);
    g_test_add_func ("/tasks/status-message", test_task_status_message);
    g_test_add_func ("/tasks/timestamps", test_task_timestamps);
    g_test_add_func ("/tasks/ttl", test_task_ttl);
    g_test_add_func ("/tasks/poll-interval", test_task_poll_interval);
    g_test_add_func ("/tasks/to-json", test_task_to_json);
    g_test_add_func ("/tasks/from-json", test_task_from_json);
    g_test_add_func ("/tasks/properties", test_task_properties);

    /* Server tasks API tests */
    g_test_add_func ("/server/tasks/add-async-tool", test_server_add_async_tool);
    g_test_add_func ("/server/tasks/list-empty", test_server_list_tasks_empty);
    g_test_add_func ("/server/tasks/get-not-found", test_server_get_task_not_found);
    g_test_add_func ("/server/tasks/cancel-not-found", test_server_cancel_task_not_found);
    g_test_add_func ("/server/tasks/update-status", test_server_update_task_status);
    g_test_add_func ("/server/tasks/complete-not-found", test_server_complete_task_not_found);
    g_test_add_func ("/server/tasks/fail-not-found", test_server_fail_task_not_found);

    return g_test_run ();
}
