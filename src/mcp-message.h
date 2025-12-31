/*
 * mcp-message.h - JSON-RPC 2.0 message types for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the JSON-RPC 2.0 message types used in MCP:
 * - McpMessage: Base abstract class for all message types
 * - McpRequest: JSON-RPC request (expects response)
 * - McpResponse: JSON-RPC success response
 * - McpErrorResponse: JSON-RPC error response
 * - McpNotification: JSON-RPC notification (no response expected)
 */

#ifndef MCP_MESSAGE_H
#define MCP_MESSAGE_H


#include <glib-object.h>
#include <json-glib/json-glib.h>
#include "mcp-enums.h"

G_BEGIN_DECLS

/**
 * MCP_JSONRPC_VERSION:
 *
 * The JSON-RPC version string used in all messages.
 */
#define MCP_JSONRPC_VERSION "2.0"

/* ========================================================================== */
/* McpMessage - Abstract base class                                           */
/* ========================================================================== */

#define MCP_TYPE_MESSAGE (mcp_message_get_type ())

G_DECLARE_DERIVABLE_TYPE (McpMessage, mcp_message, MCP, MESSAGE, GObject)

/**
 * McpMessageClass:
 * @parent_class: the parent class
 * @to_json: virtual function to serialize message to JSON
 *
 * The class structure for #McpMessage.
 */
struct _McpMessageClass
{
    GObjectClass parent_class;

    /* Virtual methods */
    JsonNode *(*to_json) (McpMessage *self);

    /*< private >*/
    gpointer padding[7];
};

/**
 * mcp_message_get_message_type:
 * @self: an #McpMessage
 *
 * Gets the type of the message.
 *
 * Returns: the #McpMessageType
 */
McpMessageType mcp_message_get_message_type (McpMessage *self);

/**
 * mcp_message_to_json:
 * @self: an #McpMessage
 *
 * Serializes the message to a JSON object.
 *
 * Returns: (transfer full): a #JsonNode containing the message
 */
JsonNode *mcp_message_to_json (McpMessage *self);

/**
 * mcp_message_to_string:
 * @self: an #McpMessage
 *
 * Serializes the message to a JSON string.
 *
 * Returns: (transfer full): a JSON string representation
 */
gchar *mcp_message_to_string (McpMessage *self);

/**
 * mcp_message_parse:
 * @json_str: a JSON string to parse
 * @error: (nullable): return location for a #GError
 *
 * Parses a JSON string and creates the appropriate message type.
 *
 * Returns: (transfer full) (nullable): the parsed #McpMessage, or %NULL on error
 */
McpMessage *mcp_message_parse (const gchar  *json_str,
                               GError      **error);

/**
 * mcp_message_new_from_json:
 * @node: a #JsonNode containing a message
 * @error: (nullable): return location for a #GError
 *
 * Creates the appropriate message type from a JSON node.
 *
 * Returns: (transfer full) (nullable): the parsed #McpMessage, or %NULL on error
 */
McpMessage *mcp_message_new_from_json (JsonNode  *node,
                                       GError   **error);

/* ========================================================================== */
/* McpRequest                                                                 */
/* ========================================================================== */

#define MCP_TYPE_REQUEST (mcp_request_get_type ())

G_DECLARE_FINAL_TYPE (McpRequest, mcp_request, MCP, REQUEST, McpMessage)

/**
 * mcp_request_new:
 * @method: the method name
 * @id: the request ID (string or numeric string)
 *
 * Creates a new JSON-RPC request.
 *
 * Returns: (transfer full): a new #McpRequest
 */
McpRequest *mcp_request_new (const gchar *method,
                             const gchar *id);

/**
 * mcp_request_new_with_params:
 * @method: the method name
 * @id: the request ID
 * @params: (nullable): the request parameters as a #JsonNode
 *
 * Creates a new JSON-RPC request with parameters.
 *
 * Returns: (transfer full): a new #McpRequest
 */
McpRequest *mcp_request_new_with_params (const gchar *method,
                                         const gchar *id,
                                         JsonNode    *params);

/**
 * mcp_request_get_id:
 * @self: an #McpRequest
 *
 * Gets the request ID.
 *
 * Returns: (transfer none): the request ID
 */
const gchar *mcp_request_get_id (McpRequest *self);

/**
 * mcp_request_get_method:
 * @self: an #McpRequest
 *
 * Gets the method name.
 *
 * Returns: (transfer none): the method name
 */
const gchar *mcp_request_get_method (McpRequest *self);

/**
 * mcp_request_get_params:
 * @self: an #McpRequest
 *
 * Gets the request parameters.
 *
 * Returns: (transfer none) (nullable): the parameters, or %NULL if none
 */
JsonNode *mcp_request_get_params (McpRequest *self);

/**
 * mcp_request_set_params:
 * @self: an #McpRequest
 * @params: (nullable): the parameters
 *
 * Sets the request parameters.
 */
void mcp_request_set_params (McpRequest *self,
                             JsonNode   *params);

/* ========================================================================== */
/* McpResponse                                                                */
/* ========================================================================== */

#define MCP_TYPE_RESPONSE (mcp_response_get_type ())

G_DECLARE_FINAL_TYPE (McpResponse, mcp_response, MCP, RESPONSE, McpMessage)

/**
 * mcp_response_new:
 * @id: the request ID this is responding to
 * @result: the result as a #JsonNode
 *
 * Creates a new JSON-RPC success response.
 *
 * Returns: (transfer full): a new #McpResponse
 */
McpResponse *mcp_response_new (const gchar *id,
                               JsonNode    *result);

/**
 * mcp_response_get_id:
 * @self: an #McpResponse
 *
 * Gets the response ID (should match the request ID).
 *
 * Returns: (transfer none): the response ID
 */
const gchar *mcp_response_get_id (McpResponse *self);

/**
 * mcp_response_get_result:
 * @self: an #McpResponse
 *
 * Gets the result.
 *
 * Returns: (transfer none): the result
 */
JsonNode *mcp_response_get_result (McpResponse *self);

/* ========================================================================== */
/* McpErrorResponse                                                           */
/* ========================================================================== */

#define MCP_TYPE_ERROR_RESPONSE (mcp_error_response_get_type ())

G_DECLARE_FINAL_TYPE (McpErrorResponse, mcp_error_response, MCP, ERROR_RESPONSE, McpMessage)

/**
 * mcp_error_response_new:
 * @id: the request ID this is responding to (can be NULL for parse errors)
 * @code: the error code
 * @message: the error message
 *
 * Creates a new JSON-RPC error response.
 *
 * Returns: (transfer full): a new #McpErrorResponse
 */
McpErrorResponse *mcp_error_response_new (const gchar *id,
                                          gint         code,
                                          const gchar *message);

/**
 * mcp_error_response_new_with_data:
 * @id: the request ID
 * @code: the error code
 * @message: the error message
 * @data: (nullable): additional error data
 *
 * Creates a new JSON-RPC error response with additional data.
 *
 * Returns: (transfer full): a new #McpErrorResponse
 */
McpErrorResponse *mcp_error_response_new_with_data (const gchar *id,
                                                    gint         code,
                                                    const gchar *message,
                                                    JsonNode    *data);

/**
 * mcp_error_response_new_from_gerror:
 * @id: the request ID
 * @error: a #GError
 *
 * Creates a new JSON-RPC error response from a GError.
 *
 * Returns: (transfer full): a new #McpErrorResponse
 */
McpErrorResponse *mcp_error_response_new_from_gerror (const gchar  *id,
                                                      const GError *error);

/**
 * mcp_error_response_get_id:
 * @self: an #McpErrorResponse
 *
 * Gets the response ID.
 *
 * Returns: (transfer none) (nullable): the response ID, or %NULL for parse errors
 */
const gchar *mcp_error_response_get_id (McpErrorResponse *self);

/**
 * mcp_error_response_get_code:
 * @self: an #McpErrorResponse
 *
 * Gets the error code.
 *
 * Returns: the error code
 */
gint mcp_error_response_get_code (McpErrorResponse *self);

/**
 * mcp_error_response_get_error_message:
 * @self: an #McpErrorResponse
 *
 * Gets the error message.
 *
 * Returns: (transfer none): the error message
 */
const gchar *mcp_error_response_get_error_message (McpErrorResponse *self);

/**
 * mcp_error_response_get_data:
 * @self: an #McpErrorResponse
 *
 * Gets the additional error data.
 *
 * Returns: (transfer none) (nullable): the error data, or %NULL if none
 */
JsonNode *mcp_error_response_get_data (McpErrorResponse *self);

/**
 * mcp_error_response_set_data:
 * @self: an #McpErrorResponse
 * @data: (nullable): the error data
 *
 * Sets the additional error data.
 */
void mcp_error_response_set_data (McpErrorResponse *self,
                                  JsonNode         *data);

/**
 * mcp_error_response_to_gerror:
 * @self: an #McpErrorResponse
 *
 * Converts the error response to a GError.
 *
 * Returns: (transfer full): a new #GError
 */
GError *mcp_error_response_to_gerror (McpErrorResponse *self);

/* ========================================================================== */
/* McpNotification                                                            */
/* ========================================================================== */

#define MCP_TYPE_NOTIFICATION (mcp_notification_get_type ())

G_DECLARE_FINAL_TYPE (McpNotification, mcp_notification, MCP, NOTIFICATION, McpMessage)

/**
 * mcp_notification_new:
 * @method: the method name
 *
 * Creates a new JSON-RPC notification.
 *
 * Returns: (transfer full): a new #McpNotification
 */
McpNotification *mcp_notification_new (const gchar *method);

/**
 * mcp_notification_new_with_params:
 * @method: the method name
 * @params: (nullable): the notification parameters
 *
 * Creates a new JSON-RPC notification with parameters.
 *
 * Returns: (transfer full): a new #McpNotification
 */
McpNotification *mcp_notification_new_with_params (const gchar *method,
                                                   JsonNode    *params);

/**
 * mcp_notification_get_method:
 * @self: an #McpNotification
 *
 * Gets the method name.
 *
 * Returns: (transfer none): the method name
 */
const gchar *mcp_notification_get_method (McpNotification *self);

/**
 * mcp_notification_get_params:
 * @self: an #McpNotification
 *
 * Gets the notification parameters.
 *
 * Returns: (transfer none) (nullable): the parameters, or %NULL if none
 */
JsonNode *mcp_notification_get_params (McpNotification *self);

/**
 * mcp_notification_set_params:
 * @self: an #McpNotification
 * @params: (nullable): the parameters
 *
 * Sets the notification parameters.
 */
void mcp_notification_set_params (McpNotification *self,
                                  JsonNode        *params);

G_END_DECLS

#endif /* MCP_MESSAGE_H */
