/*
 * mcp-root.h - Root type for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the McpRoot type used for the roots feature:
 * - McpRoot: A filesystem root exposed by the client to the server
 */

#ifndef MCP_ROOT_H
#define MCP_ROOT_H

#if !defined(MCP_INSIDE) && !defined(MCP_COMPILATION)
#error "Only <mcp/mcp.h> can be included directly."
#endif

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/**
 * McpRoot:
 *
 * A filesystem root exposed by the client to the server.
 * Roots define boundaries for server filesystem access.
 * URIs must use the file:// scheme.
 */
typedef struct _McpRoot McpRoot;

/**
 * mcp_root_new:
 * @uri: the root URI (must use file:// scheme)
 *
 * Creates a new root.
 *
 * Returns: (transfer full): a new #McpRoot
 */
McpRoot *mcp_root_new (const gchar *uri);

/**
 * mcp_root_ref:
 * @root: a #McpRoot
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpRoot
 */
McpRoot *mcp_root_ref (McpRoot *root);

/**
 * mcp_root_unref:
 * @root: a #McpRoot
 *
 * Decrements the reference count.
 */
void mcp_root_unref (McpRoot *root);

/**
 * mcp_root_get_uri:
 * @root: a #McpRoot
 *
 * Gets the root URI.
 *
 * Returns: (transfer none): the URI
 */
const gchar *mcp_root_get_uri (McpRoot *root);

/**
 * mcp_root_set_uri:
 * @root: a #McpRoot
 * @uri: the root URI
 *
 * Sets the root URI.
 */
void mcp_root_set_uri (McpRoot     *root,
                       const gchar *uri);

/**
 * mcp_root_get_name:
 * @root: a #McpRoot
 *
 * Gets the human-readable name.
 *
 * Returns: (transfer none) (nullable): the name
 */
const gchar *mcp_root_get_name (McpRoot *root);

/**
 * mcp_root_set_name:
 * @root: a #McpRoot
 * @name: (nullable): the name
 *
 * Sets the human-readable name.
 */
void mcp_root_set_name (McpRoot     *root,
                        const gchar *name);

/**
 * mcp_root_to_json:
 * @root: a #McpRoot
 *
 * Serializes the root to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_root_to_json (McpRoot *root);

/**
 * mcp_root_new_from_json:
 * @node: a #JsonNode containing a root
 * @error: (nullable): return location for a #GError
 *
 * Creates a new root from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpRoot, or %NULL on error
 */
McpRoot *mcp_root_new_from_json (JsonNode  *node,
                                 GError   **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (McpRoot, mcp_root_unref)

G_END_DECLS

#endif /* MCP_ROOT_H */
