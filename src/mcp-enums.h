/*
 * mcp-enums.h - Enumerations for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef MCP_ENUMS_H
#define MCP_ENUMS_H


#include <glib-object.h>

G_BEGIN_DECLS

/**
 * McpTransportState:
 * @MCP_TRANSPORT_STATE_DISCONNECTED: Transport is not connected
 * @MCP_TRANSPORT_STATE_CONNECTING: Transport is establishing connection
 * @MCP_TRANSPORT_STATE_CONNECTED: Transport is connected and ready
 * @MCP_TRANSPORT_STATE_DISCONNECTING: Transport is disconnecting
 * @MCP_TRANSPORT_STATE_ERROR: Transport encountered an error
 *
 * The state of an MCP transport connection.
 */
typedef enum {
    MCP_TRANSPORT_STATE_DISCONNECTED,
    MCP_TRANSPORT_STATE_CONNECTING,
    MCP_TRANSPORT_STATE_CONNECTED,
    MCP_TRANSPORT_STATE_DISCONNECTING,
    MCP_TRANSPORT_STATE_ERROR
} McpTransportState;

GType mcp_transport_state_get_type (void) G_GNUC_CONST;
#define MCP_TYPE_TRANSPORT_STATE (mcp_transport_state_get_type ())

/**
 * McpLogLevel:
 * @MCP_LOG_LEVEL_DEBUG: Debug-level message
 * @MCP_LOG_LEVEL_INFO: Informational message
 * @MCP_LOG_LEVEL_NOTICE: Normal but significant condition
 * @MCP_LOG_LEVEL_WARNING: Warning condition
 * @MCP_LOG_LEVEL_ERROR: Error condition
 * @MCP_LOG_LEVEL_CRITICAL: Critical condition
 * @MCP_LOG_LEVEL_ALERT: Action must be taken immediately
 * @MCP_LOG_LEVEL_EMERGENCY: System is unusable
 *
 * Log levels for MCP logging notifications.
 * These correspond to the logging levels defined in the MCP specification.
 */
typedef enum {
    MCP_LOG_LEVEL_DEBUG,
    MCP_LOG_LEVEL_INFO,
    MCP_LOG_LEVEL_NOTICE,
    MCP_LOG_LEVEL_WARNING,
    MCP_LOG_LEVEL_ERROR,
    MCP_LOG_LEVEL_CRITICAL,
    MCP_LOG_LEVEL_ALERT,
    MCP_LOG_LEVEL_EMERGENCY
} McpLogLevel;

GType mcp_log_level_get_type (void) G_GNUC_CONST;
#define MCP_TYPE_LOG_LEVEL (mcp_log_level_get_type ())

/**
 * McpContentType:
 * @MCP_CONTENT_TYPE_TEXT: Plain text content
 * @MCP_CONTENT_TYPE_IMAGE: Base64-encoded image content
 * @MCP_CONTENT_TYPE_AUDIO: Base64-encoded audio content
 * @MCP_CONTENT_TYPE_RESOURCE: Embedded resource content
 * @MCP_CONTENT_TYPE_RESOURCE_LINK: Link to a resource
 *
 * The type of content in an MCP message.
 */
typedef enum {
    MCP_CONTENT_TYPE_TEXT,
    MCP_CONTENT_TYPE_IMAGE,
    MCP_CONTENT_TYPE_AUDIO,
    MCP_CONTENT_TYPE_RESOURCE,
    MCP_CONTENT_TYPE_RESOURCE_LINK
} McpContentType;

GType mcp_content_type_get_type (void) G_GNUC_CONST;
#define MCP_TYPE_CONTENT_TYPE (mcp_content_type_get_type ())

/**
 * McpRole:
 * @MCP_ROLE_USER: User role in conversation
 * @MCP_ROLE_ASSISTANT: Assistant role in conversation
 *
 * The role of a participant in an MCP conversation.
 */
typedef enum {
    MCP_ROLE_USER,
    MCP_ROLE_ASSISTANT
} McpRole;

GType mcp_role_get_type (void) G_GNUC_CONST;
#define MCP_TYPE_ROLE (mcp_role_get_type ())

/**
 * McpTaskStatus:
 * @MCP_TASK_STATUS_WORKING: Task is in progress
 * @MCP_TASK_STATUS_INPUT_REQUIRED: Task requires user input
 * @MCP_TASK_STATUS_COMPLETED: Task completed successfully
 * @MCP_TASK_STATUS_FAILED: Task failed
 * @MCP_TASK_STATUS_CANCELLED: Task was cancelled
 *
 * The status of an MCP task (experimental feature).
 */
typedef enum {
    MCP_TASK_STATUS_WORKING,
    MCP_TASK_STATUS_INPUT_REQUIRED,
    MCP_TASK_STATUS_COMPLETED,
    MCP_TASK_STATUS_FAILED,
    MCP_TASK_STATUS_CANCELLED
} McpTaskStatus;

GType mcp_task_status_get_type (void) G_GNUC_CONST;
#define MCP_TYPE_TASK_STATUS (mcp_task_status_get_type ())

/**
 * McpMessageType:
 * @MCP_MESSAGE_TYPE_REQUEST: JSON-RPC request (expects response)
 * @MCP_MESSAGE_TYPE_RESPONSE: JSON-RPC response (success)
 * @MCP_MESSAGE_TYPE_ERROR: JSON-RPC error response
 * @MCP_MESSAGE_TYPE_NOTIFICATION: JSON-RPC notification (no response expected)
 *
 * The type of JSON-RPC message.
 */
typedef enum {
    MCP_MESSAGE_TYPE_REQUEST,
    MCP_MESSAGE_TYPE_RESPONSE,
    MCP_MESSAGE_TYPE_ERROR,
    MCP_MESSAGE_TYPE_NOTIFICATION
} McpMessageType;

GType mcp_message_type_get_type (void) G_GNUC_CONST;
#define MCP_TYPE_MESSAGE_TYPE (mcp_message_type_get_type ())

/**
 * mcp_log_level_to_string:
 * @level: a #McpLogLevel
 *
 * Converts a log level to its string representation as used in MCP protocol.
 *
 * Returns: (transfer none): the string name of the log level
 */
const gchar *mcp_log_level_to_string (McpLogLevel level);

/**
 * mcp_log_level_from_string:
 * @str: a string log level name
 *
 * Converts a string log level name to its enum value.
 *
 * Returns: the #McpLogLevel value, or %MCP_LOG_LEVEL_INFO if unknown
 */
McpLogLevel mcp_log_level_from_string (const gchar *str);

/**
 * mcp_role_to_string:
 * @role: a #McpRole
 *
 * Converts a role to its string representation as used in MCP protocol.
 *
 * Returns: (transfer none): the string name of the role
 */
const gchar *mcp_role_to_string (McpRole role);

/**
 * mcp_role_from_string:
 * @str: a string role name
 *
 * Converts a string role name to its enum value.
 *
 * Returns: the #McpRole value, or %MCP_ROLE_USER if unknown
 */
McpRole mcp_role_from_string (const gchar *str);

/**
 * mcp_task_status_to_string:
 * @status: a #McpTaskStatus
 *
 * Converts a task status to its string representation as used in MCP protocol.
 *
 * Returns: (transfer none): the string name of the task status
 */
const gchar *mcp_task_status_to_string (McpTaskStatus status);

/**
 * mcp_task_status_from_string:
 * @str: a string task status name
 *
 * Converts a string task status name to its enum value.
 *
 * Returns: the #McpTaskStatus value, or %MCP_TASK_STATUS_WORKING if unknown
 */
McpTaskStatus mcp_task_status_from_string (const gchar *str);

G_END_DECLS

#endif /* MCP_ENUMS_H */
