/*
 * mcp-completion.h - Completion types for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the types used for the completions feature:
 * - McpCompletionResult: The result from a completion request
 */

#ifndef MCP_COMPLETION_H
#define MCP_COMPLETION_H


#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/**
 * McpCompletionResult:
 *
 * The result from a completion request.
 * Contains a list of completion values and pagination info.
 */
typedef struct _McpCompletionResult McpCompletionResult;

/**
 * mcp_completion_result_new:
 *
 * Creates a new completion result.
 *
 * Returns: (transfer full): a new #McpCompletionResult
 */
McpCompletionResult *mcp_completion_result_new (void);

/**
 * mcp_completion_result_ref:
 * @result: a #McpCompletionResult
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpCompletionResult
 */
McpCompletionResult *mcp_completion_result_ref (McpCompletionResult *result);

/**
 * mcp_completion_result_unref:
 * @result: a #McpCompletionResult
 *
 * Decrements the reference count.
 */
void mcp_completion_result_unref (McpCompletionResult *result);

/**
 * mcp_completion_result_add_value:
 * @result: a #McpCompletionResult
 * @value: the completion value
 *
 * Adds a completion value to the result.
 */
void mcp_completion_result_add_value (McpCompletionResult *result,
                                      const gchar         *value);

/**
 * mcp_completion_result_get_values:
 * @result: a #McpCompletionResult
 *
 * Gets the list of completion values.
 *
 * Returns: (transfer none) (element-type utf8): the values list
 */
GList *mcp_completion_result_get_values (McpCompletionResult *result);

/**
 * mcp_completion_result_set_total:
 * @result: a #McpCompletionResult
 * @total: the total number of completions available
 *
 * Sets the total count of available completions.
 * Set to -1 if unknown.
 */
void mcp_completion_result_set_total (McpCompletionResult *result,
                                      gint64               total);

/**
 * mcp_completion_result_get_total:
 * @result: a #McpCompletionResult
 *
 * Gets the total count of available completions.
 *
 * Returns: the total, or -1 if unknown
 */
gint64 mcp_completion_result_get_total (McpCompletionResult *result);

/**
 * mcp_completion_result_set_has_more:
 * @result: a #McpCompletionResult
 * @has_more: whether more completions are available
 *
 * Sets whether more completions are available beyond those returned.
 */
void mcp_completion_result_set_has_more (McpCompletionResult *result,
                                         gboolean             has_more);

/**
 * mcp_completion_result_get_has_more:
 * @result: a #McpCompletionResult
 *
 * Gets whether more completions are available.
 *
 * Returns: %TRUE if more completions are available
 */
gboolean mcp_completion_result_get_has_more (McpCompletionResult *result);

/**
 * mcp_completion_result_to_json:
 * @result: a #McpCompletionResult
 *
 * Serializes the result to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_completion_result_to_json (McpCompletionResult *result);

/**
 * mcp_completion_result_new_from_json:
 * @node: a #JsonNode containing a result
 * @error: (nullable): return location for a #GError
 *
 * Creates a new completion result from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpCompletionResult, or %NULL on error
 */
McpCompletionResult *mcp_completion_result_new_from_json (JsonNode  *node,
                                                          GError   **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (McpCompletionResult, mcp_completion_result_unref)

G_END_DECLS

#endif /* MCP_COMPLETION_H */
