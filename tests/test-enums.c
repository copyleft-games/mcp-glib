/*
 * test-enums.c - Unit tests for MCP enumeration types
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include "mcp.h"

/*
 * Test that McpTransportState enum is registered and has correct values
 */
static void
test_transport_state_type (void)
{
    GType type;
    GEnumClass *enum_class;
    GEnumValue *value;

    type = MCP_TYPE_TRANSPORT_STATE;
    g_assert_true (G_TYPE_IS_ENUM (type));

    enum_class = g_type_class_ref (type);
    g_assert_nonnull (enum_class);

    /* Check that all values exist */
    value = g_enum_get_value (enum_class, MCP_TRANSPORT_STATE_DISCONNECTED);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "disconnected");

    value = g_enum_get_value (enum_class, MCP_TRANSPORT_STATE_CONNECTING);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "connecting");

    value = g_enum_get_value (enum_class, MCP_TRANSPORT_STATE_CONNECTED);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "connected");

    value = g_enum_get_value (enum_class, MCP_TRANSPORT_STATE_DISCONNECTING);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "disconnecting");

    value = g_enum_get_value (enum_class, MCP_TRANSPORT_STATE_ERROR);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "error");

    g_type_class_unref (enum_class);
}

/*
 * Test that McpLogLevel enum is registered and string conversions work
 */
static void
test_log_level_type (void)
{
    GType type;

    type = MCP_TYPE_LOG_LEVEL;
    g_assert_true (G_TYPE_IS_ENUM (type));
}

static void
test_log_level_to_string (void)
{
    g_assert_cmpstr (mcp_log_level_to_string (MCP_LOG_LEVEL_DEBUG), ==, "debug");
    g_assert_cmpstr (mcp_log_level_to_string (MCP_LOG_LEVEL_INFO), ==, "info");
    g_assert_cmpstr (mcp_log_level_to_string (MCP_LOG_LEVEL_NOTICE), ==, "notice");
    g_assert_cmpstr (mcp_log_level_to_string (MCP_LOG_LEVEL_WARNING), ==, "warning");
    g_assert_cmpstr (mcp_log_level_to_string (MCP_LOG_LEVEL_ERROR), ==, "error");
    g_assert_cmpstr (mcp_log_level_to_string (MCP_LOG_LEVEL_CRITICAL), ==, "critical");
    g_assert_cmpstr (mcp_log_level_to_string (MCP_LOG_LEVEL_ALERT), ==, "alert");
    g_assert_cmpstr (mcp_log_level_to_string (MCP_LOG_LEVEL_EMERGENCY), ==, "emergency");
}

static void
test_log_level_from_string (void)
{
    g_assert_cmpint (mcp_log_level_from_string ("debug"), ==, MCP_LOG_LEVEL_DEBUG);
    g_assert_cmpint (mcp_log_level_from_string ("info"), ==, MCP_LOG_LEVEL_INFO);
    g_assert_cmpint (mcp_log_level_from_string ("notice"), ==, MCP_LOG_LEVEL_NOTICE);
    g_assert_cmpint (mcp_log_level_from_string ("warning"), ==, MCP_LOG_LEVEL_WARNING);
    g_assert_cmpint (mcp_log_level_from_string ("error"), ==, MCP_LOG_LEVEL_ERROR);
    g_assert_cmpint (mcp_log_level_from_string ("critical"), ==, MCP_LOG_LEVEL_CRITICAL);
    g_assert_cmpint (mcp_log_level_from_string ("alert"), ==, MCP_LOG_LEVEL_ALERT);
    g_assert_cmpint (mcp_log_level_from_string ("emergency"), ==, MCP_LOG_LEVEL_EMERGENCY);

    /* Test unknown string returns INFO */
    g_assert_cmpint (mcp_log_level_from_string ("unknown"), ==, MCP_LOG_LEVEL_INFO);
    g_assert_cmpint (mcp_log_level_from_string (NULL), ==, MCP_LOG_LEVEL_INFO);
}

/*
 * Test that McpContentType enum is registered
 */
static void
test_content_type (void)
{
    GType type;
    GEnumClass *enum_class;
    GEnumValue *value;

    type = MCP_TYPE_CONTENT_TYPE;
    g_assert_true (G_TYPE_IS_ENUM (type));

    enum_class = g_type_class_ref (type);
    g_assert_nonnull (enum_class);

    value = g_enum_get_value (enum_class, MCP_CONTENT_TYPE_TEXT);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "text");

    value = g_enum_get_value (enum_class, MCP_CONTENT_TYPE_IMAGE);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "image");

    value = g_enum_get_value (enum_class, MCP_CONTENT_TYPE_AUDIO);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "audio");

    value = g_enum_get_value (enum_class, MCP_CONTENT_TYPE_RESOURCE);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "resource");

    value = g_enum_get_value (enum_class, MCP_CONTENT_TYPE_RESOURCE_LINK);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "resource_link");

    g_type_class_unref (enum_class);
}

/*
 * Test McpRole enum and string conversions
 */
static void
test_role_type (void)
{
    GType type;

    type = MCP_TYPE_ROLE;
    g_assert_true (G_TYPE_IS_ENUM (type));
}

static void
test_role_conversions (void)
{
    g_assert_cmpstr (mcp_role_to_string (MCP_ROLE_USER), ==, "user");
    g_assert_cmpstr (mcp_role_to_string (MCP_ROLE_ASSISTANT), ==, "assistant");

    g_assert_cmpint (mcp_role_from_string ("user"), ==, MCP_ROLE_USER);
    g_assert_cmpint (mcp_role_from_string ("assistant"), ==, MCP_ROLE_ASSISTANT);
    g_assert_cmpint (mcp_role_from_string ("unknown"), ==, MCP_ROLE_USER);
    g_assert_cmpint (mcp_role_from_string (NULL), ==, MCP_ROLE_USER);
}

/*
 * Test McpTaskStatus enum and string conversions
 */
static void
test_task_status_type (void)
{
    GType type;

    type = MCP_TYPE_TASK_STATUS;
    g_assert_true (G_TYPE_IS_ENUM (type));
}

static void
test_task_status_conversions (void)
{
    g_assert_cmpstr (mcp_task_status_to_string (MCP_TASK_STATUS_WORKING), ==, "working");
    g_assert_cmpstr (mcp_task_status_to_string (MCP_TASK_STATUS_INPUT_REQUIRED), ==, "input_required");
    g_assert_cmpstr (mcp_task_status_to_string (MCP_TASK_STATUS_COMPLETED), ==, "completed");
    g_assert_cmpstr (mcp_task_status_to_string (MCP_TASK_STATUS_FAILED), ==, "failed");
    g_assert_cmpstr (mcp_task_status_to_string (MCP_TASK_STATUS_CANCELLED), ==, "cancelled");

    g_assert_cmpint (mcp_task_status_from_string ("working"), ==, MCP_TASK_STATUS_WORKING);
    g_assert_cmpint (mcp_task_status_from_string ("input_required"), ==, MCP_TASK_STATUS_INPUT_REQUIRED);
    g_assert_cmpint (mcp_task_status_from_string ("completed"), ==, MCP_TASK_STATUS_COMPLETED);
    g_assert_cmpint (mcp_task_status_from_string ("failed"), ==, MCP_TASK_STATUS_FAILED);
    g_assert_cmpint (mcp_task_status_from_string ("cancelled"), ==, MCP_TASK_STATUS_CANCELLED);
    g_assert_cmpint (mcp_task_status_from_string ("unknown"), ==, MCP_TASK_STATUS_WORKING);
    g_assert_cmpint (mcp_task_status_from_string (NULL), ==, MCP_TASK_STATUS_WORKING);
}

/*
 * Test McpMessageType enum
 */
static void
test_message_type (void)
{
    GType type;
    GEnumClass *enum_class;
    GEnumValue *value;

    type = MCP_TYPE_MESSAGE_TYPE;
    g_assert_true (G_TYPE_IS_ENUM (type));

    enum_class = g_type_class_ref (type);
    g_assert_nonnull (enum_class);

    value = g_enum_get_value (enum_class, MCP_MESSAGE_TYPE_REQUEST);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "request");

    value = g_enum_get_value (enum_class, MCP_MESSAGE_TYPE_RESPONSE);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "response");

    value = g_enum_get_value (enum_class, MCP_MESSAGE_TYPE_ERROR);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "error");

    value = g_enum_get_value (enum_class, MCP_MESSAGE_TYPE_NOTIFICATION);
    g_assert_nonnull (value);
    g_assert_cmpstr (value->value_nick, ==, "notification");

    g_type_class_unref (enum_class);
}

/*
 * Test MCP error domain
 */
static void
test_error_quark (void)
{
    GQuark quark;

    quark = MCP_ERROR;
    g_assert_cmpuint (quark, !=, 0);
    g_assert_cmpstr (g_quark_to_string (quark), ==, "mcp-error-quark");
}

static void
test_error_code_is_json_rpc (void)
{
    /* JSON-RPC standard error codes should return TRUE */
    g_assert_true (mcp_error_code_is_json_rpc (MCP_ERROR_PARSE_ERROR));
    g_assert_true (mcp_error_code_is_json_rpc (MCP_ERROR_INVALID_REQUEST));
    g_assert_true (mcp_error_code_is_json_rpc (MCP_ERROR_METHOD_NOT_FOUND));
    g_assert_true (mcp_error_code_is_json_rpc (MCP_ERROR_INVALID_PARAMS));
    g_assert_true (mcp_error_code_is_json_rpc (MCP_ERROR_INTERNAL_ERROR));

    /* MCP-specific codes in JSON-RPC range should return TRUE */
    g_assert_true (mcp_error_code_is_json_rpc (MCP_ERROR_CONNECTION_CLOSED));
    g_assert_true (mcp_error_code_is_json_rpc (MCP_ERROR_TRANSPORT_ERROR));
    g_assert_true (mcp_error_code_is_json_rpc (MCP_ERROR_TIMEOUT));
    g_assert_true (mcp_error_code_is_json_rpc (MCP_ERROR_URL_ELICITATION_REQUIRED));

    /* Library-specific codes should return FALSE */
    g_assert_false (mcp_error_code_is_json_rpc (MCP_ERROR_PROTOCOL_VERSION_MISMATCH));
    g_assert_false (mcp_error_code_is_json_rpc (MCP_ERROR_NOT_INITIALIZED));
    g_assert_false (mcp_error_code_is_json_rpc (MCP_ERROR_TOOL_NOT_FOUND));
}

static void
test_error_code_conversions (void)
{
    /* JSON-RPC codes should pass through */
    g_assert_cmpint (mcp_error_code_to_json_rpc_code (MCP_ERROR_PARSE_ERROR), ==, -32700);
    g_assert_cmpint (mcp_error_code_to_json_rpc_code (MCP_ERROR_INVALID_REQUEST), ==, -32600);
    g_assert_cmpint (mcp_error_code_to_json_rpc_code (MCP_ERROR_METHOD_NOT_FOUND), ==, -32601);

    /* Library-specific errors should map appropriately */
    g_assert_cmpint (mcp_error_code_to_json_rpc_code (MCP_ERROR_TOOL_NOT_FOUND), ==, -32601);
    g_assert_cmpint (mcp_error_code_to_json_rpc_code (MCP_ERROR_RESOURCE_NOT_FOUND), ==, -32601);
    g_assert_cmpint (mcp_error_code_to_json_rpc_code (MCP_ERROR_NOT_INITIALIZED), ==, -32603);

    /* Round-trip conversions for JSON-RPC codes */
    g_assert_cmpint (mcp_error_code_from_json_rpc_code (-32700), ==, MCP_ERROR_PARSE_ERROR);
    g_assert_cmpint (mcp_error_code_from_json_rpc_code (-32600), ==, MCP_ERROR_INVALID_REQUEST);
    g_assert_cmpint (mcp_error_code_from_json_rpc_code (-32601), ==, MCP_ERROR_METHOD_NOT_FOUND);
    g_assert_cmpint (mcp_error_code_from_json_rpc_code (-32602), ==, MCP_ERROR_INVALID_PARAMS);
    g_assert_cmpint (mcp_error_code_from_json_rpc_code (-32603), ==, MCP_ERROR_INTERNAL_ERROR);
    g_assert_cmpint (mcp_error_code_from_json_rpc_code (-32000), ==, MCP_ERROR_CONNECTION_CLOSED);
    g_assert_cmpint (mcp_error_code_from_json_rpc_code (-32042), ==, MCP_ERROR_URL_ELICITATION_REQUIRED);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Transport state tests */
    g_test_add_func ("/mcp/enums/transport-state/type", test_transport_state_type);

    /* Log level tests */
    g_test_add_func ("/mcp/enums/log-level/type", test_log_level_type);
    g_test_add_func ("/mcp/enums/log-level/to-string", test_log_level_to_string);
    g_test_add_func ("/mcp/enums/log-level/from-string", test_log_level_from_string);

    /* Content type tests */
    g_test_add_func ("/mcp/enums/content-type/type", test_content_type);

    /* Role tests */
    g_test_add_func ("/mcp/enums/role/type", test_role_type);
    g_test_add_func ("/mcp/enums/role/conversions", test_role_conversions);

    /* Task status tests */
    g_test_add_func ("/mcp/enums/task-status/type", test_task_status_type);
    g_test_add_func ("/mcp/enums/task-status/conversions", test_task_status_conversions);

    /* Message type tests */
    g_test_add_func ("/mcp/enums/message-type/type", test_message_type);

    /* Error domain tests */
    g_test_add_func ("/mcp/error/quark", test_error_quark);
    g_test_add_func ("/mcp/error/is-json-rpc", test_error_code_is_json_rpc);
    g_test_add_func ("/mcp/error/conversions", test_error_code_conversions);

    return g_test_run ();
}
