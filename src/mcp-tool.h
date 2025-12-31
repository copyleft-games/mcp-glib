/*
 * mcp-tool.h - Tool definition for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef MCP_TOOL_H
#define MCP_TOOL_H


#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define MCP_TYPE_TOOL (mcp_tool_get_type ())

G_DECLARE_DERIVABLE_TYPE (McpTool, mcp_tool, MCP, TOOL, GObject)

/**
 * McpToolClass:
 * @parent_class: the parent class
 *
 * The class structure for #McpTool.
 */
struct _McpToolClass
{
    GObjectClass parent_class;

    /*< private >*/
    gpointer padding[8];
};

/**
 * mcp_tool_new:
 * @name: the tool name (must be unique within server)
 * @description: (nullable): human-readable description
 *
 * Creates a new tool definition.
 *
 * Returns: (transfer full): a new #McpTool
 */
McpTool *mcp_tool_new (const gchar *name,
                       const gchar *description);

/**
 * mcp_tool_get_name:
 * @self: an #McpTool
 *
 * Gets the programmatic name of the tool.
 *
 * Returns: (transfer none): the tool name
 */
const gchar *mcp_tool_get_name (McpTool *self);

/**
 * mcp_tool_set_name:
 * @self: an #McpTool
 * @name: the tool name
 *
 * Sets the programmatic name of the tool.
 */
void mcp_tool_set_name (McpTool     *self,
                        const gchar *name);

/**
 * mcp_tool_get_title:
 * @self: an #McpTool
 *
 * Gets the human-readable title of the tool.
 *
 * Returns: (transfer none) (nullable): the tool title
 */
const gchar *mcp_tool_get_title (McpTool *self);

/**
 * mcp_tool_set_title:
 * @self: an #McpTool
 * @title: (nullable): the tool title
 *
 * Sets the human-readable title of the tool.
 */
void mcp_tool_set_title (McpTool     *self,
                         const gchar *title);

/**
 * mcp_tool_get_description:
 * @self: an #McpTool
 *
 * Gets the description of the tool.
 *
 * Returns: (transfer none) (nullable): the tool description
 */
const gchar *mcp_tool_get_description (McpTool *self);

/**
 * mcp_tool_set_description:
 * @self: an #McpTool
 * @description: (nullable): the tool description
 *
 * Sets the description of the tool.
 */
void mcp_tool_set_description (McpTool     *self,
                               const gchar *description);

/**
 * mcp_tool_get_input_schema:
 * @self: an #McpTool
 *
 * Gets the JSON Schema defining expected input parameters.
 *
 * Returns: (transfer none) (nullable): the input schema as a #JsonNode
 */
JsonNode *mcp_tool_get_input_schema (McpTool *self);

/**
 * mcp_tool_set_input_schema:
 * @self: an #McpTool
 * @schema: (nullable): a #JsonNode containing the JSON Schema
 *
 * Sets the JSON Schema defining expected input parameters.
 */
void mcp_tool_set_input_schema (McpTool  *self,
                                JsonNode *schema);

/**
 * mcp_tool_get_output_schema:
 * @self: an #McpTool
 *
 * Gets the JSON Schema defining the tool's structured output.
 *
 * Returns: (transfer none) (nullable): the output schema as a #JsonNode
 */
JsonNode *mcp_tool_get_output_schema (McpTool *self);

/**
 * mcp_tool_set_output_schema:
 * @self: an #McpTool
 * @schema: (nullable): a #JsonNode containing the JSON Schema
 *
 * Sets the JSON Schema defining the tool's structured output.
 */
void mcp_tool_set_output_schema (McpTool  *self,
                                 JsonNode *schema);

/**
 * mcp_tool_get_read_only_hint:
 * @self: an #McpTool
 *
 * Gets whether the tool is read-only (does not modify its environment).
 *
 * Returns: %TRUE if the tool is read-only
 */
gboolean mcp_tool_get_read_only_hint (McpTool *self);

/**
 * mcp_tool_set_read_only_hint:
 * @self: an #McpTool
 * @read_only: whether the tool is read-only
 *
 * Sets whether the tool is read-only.
 */
void mcp_tool_set_read_only_hint (McpTool  *self,
                                  gboolean  read_only);

/**
 * mcp_tool_get_destructive_hint:
 * @self: an #McpTool
 *
 * Gets whether the tool may perform destructive updates.
 * Only meaningful when read_only_hint is %FALSE.
 *
 * Returns: %TRUE if the tool may be destructive (default)
 */
gboolean mcp_tool_get_destructive_hint (McpTool *self);

/**
 * mcp_tool_set_destructive_hint:
 * @self: an #McpTool
 * @destructive: whether the tool may be destructive
 *
 * Sets whether the tool may perform destructive updates.
 */
void mcp_tool_set_destructive_hint (McpTool  *self,
                                    gboolean  destructive);

/**
 * mcp_tool_get_idempotent_hint:
 * @self: an #McpTool
 *
 * Gets whether calling the tool repeatedly with the same arguments
 * has no additional effect. Only meaningful when read_only_hint is %FALSE.
 *
 * Returns: %TRUE if the tool is idempotent
 */
gboolean mcp_tool_get_idempotent_hint (McpTool *self);

/**
 * mcp_tool_set_idempotent_hint:
 * @self: an #McpTool
 * @idempotent: whether the tool is idempotent
 *
 * Sets whether the tool is idempotent.
 */
void mcp_tool_set_idempotent_hint (McpTool  *self,
                                   gboolean  idempotent);

/**
 * mcp_tool_get_open_world_hint:
 * @self: an #McpTool
 *
 * Gets whether the tool may interact with an "open world" of external entities.
 *
 * Returns: %TRUE if the tool has open world interaction (default)
 */
gboolean mcp_tool_get_open_world_hint (McpTool *self);

/**
 * mcp_tool_set_open_world_hint:
 * @self: an #McpTool
 * @open_world: whether the tool has open world interaction
 *
 * Sets whether the tool may interact with external entities.
 */
void mcp_tool_set_open_world_hint (McpTool  *self,
                                   gboolean  open_world);

/**
 * mcp_tool_to_json:
 * @self: an #McpTool
 *
 * Serializes the tool definition to a JSON object for MCP protocol.
 *
 * Returns: (transfer full): a #JsonNode containing the tool definition
 */
JsonNode *mcp_tool_to_json (McpTool *self);

/**
 * mcp_tool_new_from_json:
 * @node: a #JsonNode containing a tool definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new tool from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpTool, or %NULL on error
 */
McpTool *mcp_tool_new_from_json (JsonNode  *node,
                                 GError   **error);

G_END_DECLS

#endif /* MCP_TOOL_H */
