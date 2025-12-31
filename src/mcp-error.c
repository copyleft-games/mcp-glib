/*
 * mcp-error.c - Error domain implementation for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp.h"

/**
 * mcp_error_quark:
 *
 * Gets the MCP error quark.
 *
 * Returns: the #GQuark for MCP errors
 */
GQuark
mcp_error_quark (void)
{
    return g_quark_from_static_string ("mcp-error-quark");
}

/**
 * mcp_error_code_is_json_rpc:
 * @code: an #McpErrorCode
 *
 * Checks if the error code is a JSON-RPC standard error code.
 * JSON-RPC error codes are in the range -32768 to -32000.
 *
 * Returns: %TRUE if the code is a JSON-RPC error code
 */
gboolean
mcp_error_code_is_json_rpc (McpErrorCode code)
{
    return (code <= -32000 && code >= -32768);
}

/**
 * mcp_error_code_to_json_rpc_code:
 * @code: an #McpErrorCode
 *
 * Converts an MCP error code to its JSON-RPC numeric code for transmission.
 * For library-specific errors (positive values), returns -32603 (internal error).
 *
 * Returns: the JSON-RPC error code
 */
gint
mcp_error_code_to_json_rpc_code (McpErrorCode code)
{
    /* If it's already a JSON-RPC error code, return it directly */
    if (code <= -32000 && code >= -32768)
    {
        return (gint)code;
    }

    /*
     * Map library-specific errors to appropriate JSON-RPC codes:
     * - TOOL_NOT_FOUND, RESOURCE_NOT_FOUND, PROMPT_NOT_FOUND, TASK_NOT_FOUND
     *   -> METHOD_NOT_FOUND (-32601)
     * - NOT_INITIALIZED, ALREADY_INITIALIZED, PROTOCOL_VERSION_MISMATCH,
     *   CAPABILITY_NOT_SUPPORTED
     *   -> INTERNAL_ERROR (-32603)
     */
    switch (code)
    {
    case MCP_ERROR_TOOL_NOT_FOUND:
    case MCP_ERROR_RESOURCE_NOT_FOUND:
    case MCP_ERROR_PROMPT_NOT_FOUND:
    case MCP_ERROR_TASK_NOT_FOUND:
        return -32601; /* METHOD_NOT_FOUND */

    case MCP_ERROR_PROTOCOL_VERSION_MISMATCH:
    case MCP_ERROR_NOT_INITIALIZED:
    case MCP_ERROR_ALREADY_INITIALIZED:
    case MCP_ERROR_CAPABILITY_NOT_SUPPORTED:
    default:
        return -32603; /* INTERNAL_ERROR */
    }
}

/**
 * mcp_error_code_from_json_rpc_code:
 * @json_rpc_code: a JSON-RPC error code
 *
 * Converts a JSON-RPC numeric error code to an #McpErrorCode.
 *
 * Returns: the corresponding #McpErrorCode
 */
McpErrorCode
mcp_error_code_from_json_rpc_code (gint json_rpc_code)
{
    switch (json_rpc_code)
    {
    case -32700:
        return MCP_ERROR_PARSE_ERROR;
    case -32600:
        return MCP_ERROR_INVALID_REQUEST;
    case -32601:
        return MCP_ERROR_METHOD_NOT_FOUND;
    case -32602:
        return MCP_ERROR_INVALID_PARAMS;
    case -32603:
        return MCP_ERROR_INTERNAL_ERROR;
    case -32000:
        return MCP_ERROR_CONNECTION_CLOSED;
    case -32001:
        return MCP_ERROR_TRANSPORT_ERROR;
    case -32002:
        return MCP_ERROR_TIMEOUT;
    case -32042:
        return MCP_ERROR_URL_ELICITATION_REQUIRED;
    default:
        /* For unknown codes in the server error range, return internal error */
        if (json_rpc_code <= -32000 && json_rpc_code >= -32099)
        {
            return MCP_ERROR_INTERNAL_ERROR;
        }
        return MCP_ERROR_INTERNAL_ERROR;
    }
}
