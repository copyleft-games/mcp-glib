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

/*
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

/*
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

/*
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

/* Register the existing error codes so GIR pairs the enum with its domain. */
GType
mcp_error_get_type (void)
{
	static gsize type_id = 0;
	static const GEnumValue values[] = {
		{ MCP_ERROR_PARSE_ERROR, "MCP_ERROR_PARSE_ERROR", "parse-error" },
		{ MCP_ERROR_INVALID_REQUEST, "MCP_ERROR_INVALID_REQUEST", "invalid-request" },
		{ MCP_ERROR_METHOD_NOT_FOUND, "MCP_ERROR_METHOD_NOT_FOUND", "method-not-found" },
		{ MCP_ERROR_INVALID_PARAMS, "MCP_ERROR_INVALID_PARAMS", "invalid-params" },
		{ MCP_ERROR_INTERNAL_ERROR, "MCP_ERROR_INTERNAL_ERROR", "internal-error" },
		{ MCP_ERROR_CONNECTION_CLOSED, "MCP_ERROR_CONNECTION_CLOSED", "connection-closed" },
		{ MCP_ERROR_TRANSPORT_ERROR, "MCP_ERROR_TRANSPORT_ERROR", "transport-error" },
		{ MCP_ERROR_TIMEOUT, "MCP_ERROR_TIMEOUT", "timeout" },
		{ MCP_ERROR_URL_ELICITATION_REQUIRED, "MCP_ERROR_URL_ELICITATION_REQUIRED", "url-elicitation-required" },
		{ MCP_ERROR_PROTOCOL_VERSION_MISMATCH, "MCP_ERROR_PROTOCOL_VERSION_MISMATCH", "protocol-version-mismatch" },
		{ MCP_ERROR_NOT_INITIALIZED, "MCP_ERROR_NOT_INITIALIZED", "not-initialized" },
		{ MCP_ERROR_ALREADY_INITIALIZED, "MCP_ERROR_ALREADY_INITIALIZED", "already-initialized" },
		{ MCP_ERROR_TOOL_NOT_FOUND, "MCP_ERROR_TOOL_NOT_FOUND", "tool-not-found" },
		{ MCP_ERROR_RESOURCE_NOT_FOUND, "MCP_ERROR_RESOURCE_NOT_FOUND", "resource-not-found" },
		{ MCP_ERROR_PROMPT_NOT_FOUND, "MCP_ERROR_PROMPT_NOT_FOUND", "prompt-not-found" },
		{ MCP_ERROR_TASK_NOT_FOUND, "MCP_ERROR_TASK_NOT_FOUND", "task-not-found" },
		{ MCP_ERROR_CAPABILITY_NOT_SUPPORTED, "MCP_ERROR_CAPABILITY_NOT_SUPPORTED", "capability-not-supported" },
		{ 0, NULL, NULL }
	};

	if (g_once_init_enter (&type_id))
	{
		GType registered_type;

		registered_type = g_enum_register_static ("McpErrorCode", values);
		g_once_init_leave (&type_id, registered_type);
	}

	return (GType) type_id;
}
