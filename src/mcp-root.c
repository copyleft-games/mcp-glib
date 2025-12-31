/*
 * mcp-root.c - Root type implementation for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp.h"

struct _McpRoot
{
    gint   ref_count;
    gchar *uri;
    gchar *name;
};

/**
 * mcp_root_new:
 * @uri: the root URI (must use file:// scheme)
 *
 * Creates a new root.
 *
 * Returns: (transfer full): a new #McpRoot
 */
McpRoot *
mcp_root_new (const gchar *uri)
{
    McpRoot *root;

    g_return_val_if_fail (uri != NULL, NULL);

    root = g_new0 (McpRoot, 1);
    root->ref_count = 1;
    root->uri = g_strdup (uri);

    return root;
}

/**
 * mcp_root_ref:
 * @root: a #McpRoot
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpRoot
 */
McpRoot *
mcp_root_ref (McpRoot *root)
{
    g_return_val_if_fail (root != NULL, NULL);

    root->ref_count++;
    return root;
}

/**
 * mcp_root_unref:
 * @root: a #McpRoot
 *
 * Decrements the reference count.
 */
void
mcp_root_unref (McpRoot *root)
{
    g_return_if_fail (root != NULL);

    if (--root->ref_count == 0)
    {
        g_free (root->uri);
        g_free (root->name);
        g_free (root);
    }
}

/**
 * mcp_root_get_uri:
 * @root: a #McpRoot
 *
 * Gets the root URI.
 *
 * Returns: (transfer none): the URI
 */
const gchar *
mcp_root_get_uri (McpRoot *root)
{
    g_return_val_if_fail (root != NULL, NULL);
    return root->uri;
}

/**
 * mcp_root_set_uri:
 * @root: a #McpRoot
 * @uri: the root URI
 *
 * Sets the root URI.
 */
void
mcp_root_set_uri (McpRoot     *root,
                  const gchar *uri)
{
    g_return_if_fail (root != NULL);
    g_return_if_fail (uri != NULL);

    g_free (root->uri);
    root->uri = g_strdup (uri);
}

/**
 * mcp_root_get_name:
 * @root: a #McpRoot
 *
 * Gets the human-readable name.
 *
 * Returns: (transfer none) (nullable): the name
 */
const gchar *
mcp_root_get_name (McpRoot *root)
{
    g_return_val_if_fail (root != NULL, NULL);
    return root->name;
}

/**
 * mcp_root_set_name:
 * @root: a #McpRoot
 * @name: (nullable): the name
 *
 * Sets the human-readable name.
 */
void
mcp_root_set_name (McpRoot     *root,
                   const gchar *name)
{
    g_return_if_fail (root != NULL);

    g_free (root->name);
    root->name = g_strdup (name);
}

/**
 * mcp_root_to_json:
 * @root: a #McpRoot
 *
 * Serializes the root to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *
mcp_root_to_json (McpRoot *root)
{
    JsonBuilder *builder;
    JsonNode *node;

    g_return_val_if_fail (root != NULL, NULL);

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "uri");
    json_builder_add_string_value (builder, root->uri);

    if (root->name != NULL)
    {
        json_builder_set_member_name (builder, "name");
        json_builder_add_string_value (builder, root->name);
    }

    json_builder_end_object (builder);
    node = json_builder_get_root (builder);
    g_object_unref (builder);

    return node;
}

/**
 * mcp_root_new_from_json:
 * @node: a #JsonNode containing a root
 * @error: (nullable): return location for a #GError
 *
 * Creates a new root from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpRoot, or %NULL on error
 */
McpRoot *
mcp_root_new_from_json (JsonNode  *node,
                        GError   **error)
{
    JsonObject *obj;
    McpRoot *root;
    const gchar *uri;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Root must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    /* Required: uri */
    if (!json_object_has_member (obj, "uri"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Root missing required 'uri' field");
        return NULL;
    }
    uri = json_object_get_string_member (obj, "uri");

    root = mcp_root_new (uri);

    /* Optional: name */
    if (json_object_has_member (obj, "name"))
    {
        mcp_root_set_name (root, json_object_get_string_member (obj, "name"));
    }

    return root;
}
