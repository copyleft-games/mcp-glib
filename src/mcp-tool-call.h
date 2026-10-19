/*
 * mcp-tool-call.h - A tools/call request answered later
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines McpToolCall, the handle a deferred tool handler
 * (mcp_server_add_deferred_tool()) receives instead of returning a
 * result at once.  The handler keeps a reference, lets the main loop
 * run, and answers when the work is done with
 * mcp_tool_call_return_result() or mcp_tool_call_return_error(); in the
 * meantime it may report progress (notifications/progress, when the
 * client asked for it with a progress token) and learns about the
 * client's notifications/cancelled through #McpToolCall::cancelled and
 * the call's #GCancellable.
 */

#ifndef MCP_TOOL_CALL_H
#define MCP_TOOL_CALL_H


#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include "mcp-tool-provider.h"

G_BEGIN_DECLS

#define MCP_TYPE_TOOL_CALL (mcp_tool_call_get_type ())

G_DECLARE_FINAL_TYPE (McpToolCall, mcp_tool_call, MCP, TOOL_CALL, GObject)

/**
 * mcp_tool_call_get_name:
 * @self: an #McpToolCall
 *
 * Gets the name of the tool called.
 *
 * Returns: (transfer none): the tool name
 */
const gchar *mcp_tool_call_get_name (McpToolCall *self);

/**
 * mcp_tool_call_get_arguments:
 * @self: an #McpToolCall
 *
 * Gets the arguments the client passed.
 *
 * Returns: (transfer none) (nullable): the arguments object
 */
JsonObject *mcp_tool_call_get_arguments (McpToolCall *self);

/**
 * mcp_tool_call_get_request_id:
 * @self: an #McpToolCall
 *
 * Gets the JSON-RPC id of the tools/call request (a number is given as
 * its decimal text).
 *
 * Returns: (transfer none): the request id
 */
const gchar *mcp_tool_call_get_request_id (McpToolCall *self);

/**
 * mcp_tool_call_has_progress_token:
 * @self: an #McpToolCall
 *
 * Whether the client asked for progress (`_meta.progressToken` in the
 * request).  Without a token mcp_tool_call_send_progress() does nothing.
 *
 * Returns: %TRUE when progress notifications reach the client
 */
gboolean mcp_tool_call_has_progress_token (McpToolCall *self);

/**
 * mcp_tool_call_get_cancellable:
 * @self: an #McpToolCall
 *
 * Gets a #GCancellable that is cancelled when the client cancels the
 * request or the connection goes away; hand it to the work the call
 * started.
 *
 * Returns: (transfer none): the cancellable
 */
GCancellable *mcp_tool_call_get_cancellable (McpToolCall *self);

/**
 * mcp_tool_call_is_completed:
 * @self: an #McpToolCall
 *
 * Returns: %TRUE once a result or an error was returned
 */
gboolean mcp_tool_call_is_completed (McpToolCall *self);

/**
 * mcp_tool_call_is_cancelled:
 * @self: an #McpToolCall
 *
 * Returns: %TRUE when the client cancelled the request or the
 *   connection closed before the call completed
 */
gboolean mcp_tool_call_is_cancelled (McpToolCall *self);

/**
 * mcp_tool_call_send_progress:
 * @self: an #McpToolCall
 * @progress: the progress so far (it must grow from one call to the next)
 * @total: the total, or a value of 0 or less when it is not known
 * @message: (nullable): a human-readable description of the step
 *
 * Sends notifications/progress for this call with the client's progress
 * token.  Does nothing when the client gave no token, or once the call
 * completed or was cancelled.
 */
void mcp_tool_call_send_progress (McpToolCall *self,
                                  gdouble      progress,
                                  gdouble      total,
                                  const gchar *message);

/**
 * mcp_tool_call_return_result:
 * @self: an #McpToolCall
 * @result: (transfer none): the tool result
 *
 * Answers the request with @result.  A call completes once: later
 * returns are ignored, and so is a return after the client cancelled
 * (the protocol forbids answering a cancelled request).
 */
void mcp_tool_call_return_result (McpToolCall   *self,
                                  McpToolResult *result);

/**
 * mcp_tool_call_return_error:
 * @self: an #McpToolCall
 * @code: a JSON-RPC error code (an #McpErrorCode)
 * @message: the error message
 *
 * Answers the request with a JSON-RPC error.  Tool failures the model
 * should see are better a result with isError set
 * (mcp_tool_call_return_result()); this is for protocol errors.
 */
void mcp_tool_call_return_error (McpToolCall *self,
                                 gint         code,
                                 const gchar *message);

G_END_DECLS

#endif /* MCP_TOOL_CALL_H */
