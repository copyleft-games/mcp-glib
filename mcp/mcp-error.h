/*
 * mcp-error.h - Error domain and codes for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef MCP_ERROR_H
#define MCP_ERROR_H

#if !defined(MCP_INSIDE) && !defined(MCP_COMPILATION)
#error "Only <mcp/mcp.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

/**
 * MCP_ERROR:
 *
 * Error domain for MCP operations. Errors in this domain will be from
 * the #McpErrorCode enumeration.
 */
#define MCP_ERROR (mcp_error_quark ())

GQuark mcp_error_quark (void);

/**
 * McpErrorCode:
 * @MCP_ERROR_PARSE_ERROR: Invalid JSON was received (JSON-RPC -32700)
 * @MCP_ERROR_INVALID_REQUEST: The JSON sent is not a valid Request object (JSON-RPC -32600)
 * @MCP_ERROR_METHOD_NOT_FOUND: The method does not exist or is not available (JSON-RPC -32601)
 * @MCP_ERROR_INVALID_PARAMS: Invalid method parameters (JSON-RPC -32602)
 * @MCP_ERROR_INTERNAL_ERROR: Internal JSON-RPC error (JSON-RPC -32603)
 * @MCP_ERROR_CONNECTION_CLOSED: The connection was closed (MCP -32000)
 * @MCP_ERROR_TRANSPORT_ERROR: Transport-level error occurred (MCP -32001)
 * @MCP_ERROR_TIMEOUT: Operation timed out (MCP -32002)
 * @MCP_ERROR_URL_ELICITATION_REQUIRED: URL mode elicitation required (MCP -32042)
 * @MCP_ERROR_PROTOCOL_VERSION_MISMATCH: Protocol version negotiation failed
 * @MCP_ERROR_NOT_INITIALIZED: Operation attempted before initialization
 * @MCP_ERROR_ALREADY_INITIALIZED: Attempted to initialize twice
 * @MCP_ERROR_TOOL_NOT_FOUND: Requested tool does not exist
 * @MCP_ERROR_RESOURCE_NOT_FOUND: Requested resource does not exist
 * @MCP_ERROR_PROMPT_NOT_FOUND: Requested prompt does not exist
 * @MCP_ERROR_TASK_NOT_FOUND: Requested task does not exist
 * @MCP_ERROR_CAPABILITY_NOT_SUPPORTED: Required capability not supported
 *
 * Error codes for MCP operations. The negative values correspond to
 * JSON-RPC 2.0 and MCP protocol error codes.
 */
typedef enum {
    /* JSON-RPC 2.0 standard error codes */
    MCP_ERROR_PARSE_ERROR            = -32700,
    MCP_ERROR_INVALID_REQUEST        = -32600,
    MCP_ERROR_METHOD_NOT_FOUND       = -32601,
    MCP_ERROR_INVALID_PARAMS         = -32602,
    MCP_ERROR_INTERNAL_ERROR         = -32603,

    /* MCP-specific error codes (in range -32000 to -32099) */
    MCP_ERROR_CONNECTION_CLOSED      = -32000,
    MCP_ERROR_TRANSPORT_ERROR        = -32001,
    MCP_ERROR_TIMEOUT                = -32002,
    MCP_ERROR_URL_ELICITATION_REQUIRED = -32042,

    /* Library-specific error codes (positive values) */
    MCP_ERROR_PROTOCOL_VERSION_MISMATCH = 1,
    MCP_ERROR_NOT_INITIALIZED,
    MCP_ERROR_ALREADY_INITIALIZED,
    MCP_ERROR_TOOL_NOT_FOUND,
    MCP_ERROR_RESOURCE_NOT_FOUND,
    MCP_ERROR_PROMPT_NOT_FOUND,
    MCP_ERROR_TASK_NOT_FOUND,
    MCP_ERROR_CAPABILITY_NOT_SUPPORTED
} McpErrorCode;

/**
 * mcp_error_code_is_json_rpc:
 * @code: an #McpErrorCode
 *
 * Checks if the error code is a JSON-RPC standard error code.
 *
 * Returns: %TRUE if the code is a JSON-RPC error code
 */
gboolean mcp_error_code_is_json_rpc (McpErrorCode code);

/**
 * mcp_error_code_to_json_rpc_code:
 * @code: an #McpErrorCode
 *
 * Converts an MCP error code to its JSON-RPC numeric code for transmission.
 * For library-specific errors, returns -32603 (internal error).
 *
 * Returns: the JSON-RPC error code
 */
gint mcp_error_code_to_json_rpc_code (McpErrorCode code);

/**
 * mcp_error_code_from_json_rpc_code:
 * @json_rpc_code: a JSON-RPC error code
 *
 * Converts a JSON-RPC numeric error code to an #McpErrorCode.
 *
 * Returns: the corresponding #McpErrorCode
 */
McpErrorCode mcp_error_code_from_json_rpc_code (gint json_rpc_code);

G_END_DECLS

#endif /* MCP_ERROR_H */
