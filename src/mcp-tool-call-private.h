/*
 * mcp-tool-call-private.h - McpToolCall's link to its server (not installed)
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef MCP_TOOL_CALL_PRIVATE_H
#define MCP_TOOL_CALL_PRIVATE_H

#include "mcp-tool-call.h"
#include "mcp-message.h"
#include "mcp-server.h"

G_BEGIN_DECLS

/* a call for @request on @server (weakly held); @progress_token copied */
McpToolCall *_mcp_tool_call_new (McpServer   *server,
                                 McpRequest  *request,
                                 const gchar *name,
                                 JsonObject  *arguments,
                                 JsonNode    *progress_token);

/* the request the call answers */
McpRequest *_mcp_tool_call_get_request (McpToolCall *self);

/* the client cancelled, or the connection went: no answer will be sent */
void _mcp_tool_call_cancel (McpToolCall *self);

/* McpServer: answers @call (exactly one of @result and @error_message),
 * and forgets it */
void _mcp_server_finish_tool_call (McpServer   *server,
                                   McpToolCall *call,
                                   JsonNode    *result,
                                   gint         error_code,
                                   const gchar *error_message);

/* McpServer: sends notifications/progress with @params (taken) */
void _mcp_server_send_tool_call_progress (McpServer *server,
                                          JsonNode  *params);

G_END_DECLS

#endif /* MCP_TOOL_CALL_PRIVATE_H */
