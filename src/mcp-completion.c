/*
 * mcp-completion.c - Completion types implementation for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define MCP_COMPILATION
#include <mcp/mcp.h>

struct _McpCompletionResult
{
    gint      ref_count;
    GList    *values;      /* List of gchar* completion values */
    gint64    total;       /* Total available completions, -1 if unknown */
    gboolean  has_more;    /* Whether more completions are available */
};

/**
 * mcp_completion_result_new:
 *
 * Creates a new completion result.
 *
 * Returns: (transfer full): a new #McpCompletionResult
 */
McpCompletionResult *
mcp_completion_result_new (void)
{
    McpCompletionResult *result;

    result = g_new0 (McpCompletionResult, 1);
    result->ref_count = 1;
    result->values = NULL;
    result->total = -1;
    result->has_more = FALSE;

    return result;
}

/**
 * mcp_completion_result_ref:
 * @result: a #McpCompletionResult
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpCompletionResult
 */
McpCompletionResult *
mcp_completion_result_ref (McpCompletionResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);

    result->ref_count++;
    return result;
}

/**
 * mcp_completion_result_unref:
 * @result: a #McpCompletionResult
 *
 * Decrements the reference count.
 */
void
mcp_completion_result_unref (McpCompletionResult *result)
{
    g_return_if_fail (result != NULL);

    if (--result->ref_count == 0)
    {
        g_list_free_full (result->values, g_free);
        g_free (result);
    }
}

/**
 * mcp_completion_result_add_value:
 * @result: a #McpCompletionResult
 * @value: the completion value
 *
 * Adds a completion value to the result.
 */
void
mcp_completion_result_add_value (McpCompletionResult *result,
                                 const gchar         *value)
{
    g_return_if_fail (result != NULL);
    g_return_if_fail (value != NULL);

    result->values = g_list_append (result->values, g_strdup (value));
}

/**
 * mcp_completion_result_get_values:
 * @result: a #McpCompletionResult
 *
 * Gets the list of completion values.
 *
 * Returns: (transfer none) (element-type utf8): the values list
 */
GList *
mcp_completion_result_get_values (McpCompletionResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);
    return result->values;
}

/**
 * mcp_completion_result_set_total:
 * @result: a #McpCompletionResult
 * @total: the total number of completions available
 *
 * Sets the total count of available completions.
 * Set to -1 if unknown.
 */
void
mcp_completion_result_set_total (McpCompletionResult *result,
                                 gint64               total)
{
    g_return_if_fail (result != NULL);
    result->total = total;
}

/**
 * mcp_completion_result_get_total:
 * @result: a #McpCompletionResult
 *
 * Gets the total count of available completions.
 *
 * Returns: the total, or -1 if unknown
 */
gint64
mcp_completion_result_get_total (McpCompletionResult *result)
{
    g_return_val_if_fail (result != NULL, -1);
    return result->total;
}

/**
 * mcp_completion_result_set_has_more:
 * @result: a #McpCompletionResult
 * @has_more: whether more completions are available
 *
 * Sets whether more completions are available beyond those returned.
 */
void
mcp_completion_result_set_has_more (McpCompletionResult *result,
                                    gboolean             has_more)
{
    g_return_if_fail (result != NULL);
    result->has_more = has_more;
}

/**
 * mcp_completion_result_get_has_more:
 * @result: a #McpCompletionResult
 *
 * Gets whether more completions are available.
 *
 * Returns: %TRUE if more completions are available
 */
gboolean
mcp_completion_result_get_has_more (McpCompletionResult *result)
{
    g_return_val_if_fail (result != NULL, FALSE);
    return result->has_more;
}

/**
 * mcp_completion_result_to_json:
 * @result: a #McpCompletionResult
 *
 * Serializes the result to JSON.
 * Format follows MCP spec: { "completion": { "values": [...], "total": N, "hasMore": bool } }
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *
mcp_completion_result_to_json (McpCompletionResult *result)
{
    JsonBuilder *builder;
    JsonNode *node;
    GList *l;

    g_return_val_if_fail (result != NULL, NULL);

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    /* completion object */
    json_builder_set_member_name (builder, "completion");
    json_builder_begin_object (builder);

    /* values array */
    json_builder_set_member_name (builder, "values");
    json_builder_begin_array (builder);
    for (l = result->values; l != NULL; l = l->next)
    {
        json_builder_add_string_value (builder, (const gchar *)l->data);
    }
    json_builder_end_array (builder);

    /* total (optional, only if known) */
    if (result->total >= 0)
    {
        json_builder_set_member_name (builder, "total");
        json_builder_add_int_value (builder, result->total);
    }

    /* hasMore (optional) */
    if (result->has_more)
    {
        json_builder_set_member_name (builder, "hasMore");
        json_builder_add_boolean_value (builder, TRUE);
    }

    json_builder_end_object (builder);  /* completion */
    json_builder_end_object (builder);  /* root */

    node = json_builder_get_root (builder);
    g_object_unref (builder);

    return node;
}

/**
 * mcp_completion_result_new_from_json:
 * @node: a #JsonNode containing a result
 * @error: (nullable): return location for a #GError
 *
 * Creates a new completion result from JSON.
 * Expects format: { "completion": { "values": [...], "total": N, "hasMore": bool } }
 *
 * Returns: (transfer full) (nullable): a new #McpCompletionResult, or %NULL on error
 */
McpCompletionResult *
mcp_completion_result_new_from_json (JsonNode  *node,
                                     GError   **error)
{
    JsonObject *obj;
    JsonObject *completion_obj;
    JsonArray *values_array;
    McpCompletionResult *result;
    guint i;
    guint len;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Completion result must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    /* Get completion object */
    if (!json_object_has_member (obj, "completion"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Completion result missing 'completion' field");
        return NULL;
    }

    completion_obj = json_object_get_object_member (obj, "completion");
    if (completion_obj == NULL)
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "'completion' field must be an object");
        return NULL;
    }

    /* Get values array */
    if (!json_object_has_member (completion_obj, "values"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Completion result missing 'values' field");
        return NULL;
    }

    values_array = json_object_get_array_member (completion_obj, "values");
    if (values_array == NULL)
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "'values' field must be an array");
        return NULL;
    }

    result = mcp_completion_result_new ();

    /* Parse values */
    len = json_array_get_length (values_array);
    for (i = 0; i < len; i++)
    {
        const gchar *value;

        value = json_array_get_string_element (values_array, i);
        if (value != NULL)
        {
            mcp_completion_result_add_value (result, value);
        }
    }

    /* Optional: total */
    if (json_object_has_member (completion_obj, "total"))
    {
        result->total = json_object_get_int_member (completion_obj, "total");
    }

    /* Optional: hasMore */
    if (json_object_has_member (completion_obj, "hasMore"))
    {
        result->has_more = json_object_get_boolean_member (completion_obj, "hasMore");
    }

    return result;
}
