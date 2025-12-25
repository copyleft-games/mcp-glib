/*
 * mcp-enums.c - Enumeration implementations for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define MCP_COMPILATION
#include <mcp/mcp.h>

/*
 * McpTransportState GType registration
 */
GType
mcp_transport_state_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { MCP_TRANSPORT_STATE_DISCONNECTED, "MCP_TRANSPORT_STATE_DISCONNECTED", "disconnected" },
            { MCP_TRANSPORT_STATE_CONNECTING, "MCP_TRANSPORT_STATE_CONNECTING", "connecting" },
            { MCP_TRANSPORT_STATE_CONNECTED, "MCP_TRANSPORT_STATE_CONNECTED", "connected" },
            { MCP_TRANSPORT_STATE_DISCONNECTING, "MCP_TRANSPORT_STATE_DISCONNECTING", "disconnecting" },
            { MCP_TRANSPORT_STATE_ERROR, "MCP_TRANSPORT_STATE_ERROR", "error" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("McpTransportState", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * McpLogLevel GType registration
 */
GType
mcp_log_level_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { MCP_LOG_LEVEL_DEBUG, "MCP_LOG_LEVEL_DEBUG", "debug" },
            { MCP_LOG_LEVEL_INFO, "MCP_LOG_LEVEL_INFO", "info" },
            { MCP_LOG_LEVEL_NOTICE, "MCP_LOG_LEVEL_NOTICE", "notice" },
            { MCP_LOG_LEVEL_WARNING, "MCP_LOG_LEVEL_WARNING", "warning" },
            { MCP_LOG_LEVEL_ERROR, "MCP_LOG_LEVEL_ERROR", "error" },
            { MCP_LOG_LEVEL_CRITICAL, "MCP_LOG_LEVEL_CRITICAL", "critical" },
            { MCP_LOG_LEVEL_ALERT, "MCP_LOG_LEVEL_ALERT", "alert" },
            { MCP_LOG_LEVEL_EMERGENCY, "MCP_LOG_LEVEL_EMERGENCY", "emergency" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("McpLogLevel", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * McpContentType GType registration
 */
GType
mcp_content_type_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { MCP_CONTENT_TYPE_TEXT, "MCP_CONTENT_TYPE_TEXT", "text" },
            { MCP_CONTENT_TYPE_IMAGE, "MCP_CONTENT_TYPE_IMAGE", "image" },
            { MCP_CONTENT_TYPE_AUDIO, "MCP_CONTENT_TYPE_AUDIO", "audio" },
            { MCP_CONTENT_TYPE_RESOURCE, "MCP_CONTENT_TYPE_RESOURCE", "resource" },
            { MCP_CONTENT_TYPE_RESOURCE_LINK, "MCP_CONTENT_TYPE_RESOURCE_LINK", "resource_link" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("McpContentType", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * McpRole GType registration
 */
GType
mcp_role_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { MCP_ROLE_USER, "MCP_ROLE_USER", "user" },
            { MCP_ROLE_ASSISTANT, "MCP_ROLE_ASSISTANT", "assistant" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("McpRole", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * McpTaskStatus GType registration
 */
GType
mcp_task_status_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { MCP_TASK_STATUS_WORKING, "MCP_TASK_STATUS_WORKING", "working" },
            { MCP_TASK_STATUS_INPUT_REQUIRED, "MCP_TASK_STATUS_INPUT_REQUIRED", "input_required" },
            { MCP_TASK_STATUS_COMPLETED, "MCP_TASK_STATUS_COMPLETED", "completed" },
            { MCP_TASK_STATUS_FAILED, "MCP_TASK_STATUS_FAILED", "failed" },
            { MCP_TASK_STATUS_CANCELLED, "MCP_TASK_STATUS_CANCELLED", "cancelled" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("McpTaskStatus", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * McpMessageType GType registration
 */
GType
mcp_message_type_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { MCP_MESSAGE_TYPE_REQUEST, "MCP_MESSAGE_TYPE_REQUEST", "request" },
            { MCP_MESSAGE_TYPE_RESPONSE, "MCP_MESSAGE_TYPE_RESPONSE", "response" },
            { MCP_MESSAGE_TYPE_ERROR, "MCP_MESSAGE_TYPE_ERROR", "error" },
            { MCP_MESSAGE_TYPE_NOTIFICATION, "MCP_MESSAGE_TYPE_NOTIFICATION", "notification" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id;

        g_define_type_id = g_enum_register_static ("McpMessageType", values);
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*
 * String conversion utilities
 */

/**
 * mcp_log_level_to_string:
 * @level: a #McpLogLevel
 *
 * Converts a log level to its string representation as used in MCP protocol.
 *
 * Returns: (transfer none): the string name of the log level
 */
const gchar *
mcp_log_level_to_string (McpLogLevel level)
{
    switch (level)
    {
    case MCP_LOG_LEVEL_DEBUG:
        return "debug";
    case MCP_LOG_LEVEL_INFO:
        return "info";
    case MCP_LOG_LEVEL_NOTICE:
        return "notice";
    case MCP_LOG_LEVEL_WARNING:
        return "warning";
    case MCP_LOG_LEVEL_ERROR:
        return "error";
    case MCP_LOG_LEVEL_CRITICAL:
        return "critical";
    case MCP_LOG_LEVEL_ALERT:
        return "alert";
    case MCP_LOG_LEVEL_EMERGENCY:
        return "emergency";
    default:
        return "info";
    }
}

/**
 * mcp_log_level_from_string:
 * @str: a string log level name
 *
 * Converts a string log level name to its enum value.
 *
 * Returns: the #McpLogLevel value, or %MCP_LOG_LEVEL_INFO if unknown
 */
McpLogLevel
mcp_log_level_from_string (const gchar *str)
{
    if (str == NULL)
        return MCP_LOG_LEVEL_INFO;

    if (g_strcmp0 (str, "debug") == 0)
        return MCP_LOG_LEVEL_DEBUG;
    if (g_strcmp0 (str, "info") == 0)
        return MCP_LOG_LEVEL_INFO;
    if (g_strcmp0 (str, "notice") == 0)
        return MCP_LOG_LEVEL_NOTICE;
    if (g_strcmp0 (str, "warning") == 0)
        return MCP_LOG_LEVEL_WARNING;
    if (g_strcmp0 (str, "error") == 0)
        return MCP_LOG_LEVEL_ERROR;
    if (g_strcmp0 (str, "critical") == 0)
        return MCP_LOG_LEVEL_CRITICAL;
    if (g_strcmp0 (str, "alert") == 0)
        return MCP_LOG_LEVEL_ALERT;
    if (g_strcmp0 (str, "emergency") == 0)
        return MCP_LOG_LEVEL_EMERGENCY;

    return MCP_LOG_LEVEL_INFO;
}

/**
 * mcp_role_to_string:
 * @role: a #McpRole
 *
 * Converts a role to its string representation as used in MCP protocol.
 *
 * Returns: (transfer none): the string name of the role
 */
const gchar *
mcp_role_to_string (McpRole role)
{
    switch (role)
    {
    case MCP_ROLE_USER:
        return "user";
    case MCP_ROLE_ASSISTANT:
        return "assistant";
    default:
        return "user";
    }
}

/**
 * mcp_role_from_string:
 * @str: a string role name
 *
 * Converts a string role name to its enum value.
 *
 * Returns: the #McpRole value, or %MCP_ROLE_USER if unknown
 */
McpRole
mcp_role_from_string (const gchar *str)
{
    if (str == NULL)
        return MCP_ROLE_USER;

    if (g_strcmp0 (str, "user") == 0)
        return MCP_ROLE_USER;
    if (g_strcmp0 (str, "assistant") == 0)
        return MCP_ROLE_ASSISTANT;

    return MCP_ROLE_USER;
}

/**
 * mcp_task_status_to_string:
 * @status: a #McpTaskStatus
 *
 * Converts a task status to its string representation as used in MCP protocol.
 *
 * Returns: (transfer none): the string name of the task status
 */
const gchar *
mcp_task_status_to_string (McpTaskStatus status)
{
    switch (status)
    {
    case MCP_TASK_STATUS_WORKING:
        return "working";
    case MCP_TASK_STATUS_INPUT_REQUIRED:
        return "input_required";
    case MCP_TASK_STATUS_COMPLETED:
        return "completed";
    case MCP_TASK_STATUS_FAILED:
        return "failed";
    case MCP_TASK_STATUS_CANCELLED:
        return "cancelled";
    default:
        return "working";
    }
}

/**
 * mcp_task_status_from_string:
 * @str: a string task status name
 *
 * Converts a string task status name to its enum value.
 *
 * Returns: the #McpTaskStatus value, or %MCP_TASK_STATUS_WORKING if unknown
 */
McpTaskStatus
mcp_task_status_from_string (const gchar *str)
{
    if (str == NULL)
        return MCP_TASK_STATUS_WORKING;

    if (g_strcmp0 (str, "working") == 0)
        return MCP_TASK_STATUS_WORKING;
    if (g_strcmp0 (str, "input_required") == 0)
        return MCP_TASK_STATUS_INPUT_REQUIRED;
    if (g_strcmp0 (str, "completed") == 0)
        return MCP_TASK_STATUS_COMPLETED;
    if (g_strcmp0 (str, "failed") == 0)
        return MCP_TASK_STATUS_FAILED;
    if (g_strcmp0 (str, "cancelled") == 0)
        return MCP_TASK_STATUS_CANCELLED;

    return MCP_TASK_STATUS_WORKING;
}
