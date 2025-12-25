/*
 * mcp-prompt.h - Prompt definition for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef MCP_PROMPT_H
#define MCP_PROMPT_H

#if !defined(MCP_INSIDE) && !defined(MCP_COMPILATION)
#error "Only <mcp/mcp.h> can be included directly."
#endif

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/* ========================================================================== */
/* McpPromptArgument - Boxed type for prompt arguments                        */
/* ========================================================================== */

#define MCP_TYPE_PROMPT_ARGUMENT (mcp_prompt_argument_get_type ())

typedef struct _McpPromptArgument McpPromptArgument;

GType mcp_prompt_argument_get_type (void) G_GNUC_CONST;

/**
 * mcp_prompt_argument_new:
 * @name: the argument name
 * @description: (nullable): human-readable description
 * @required: whether the argument is required
 *
 * Creates a new prompt argument definition.
 *
 * Returns: (transfer full): a new #McpPromptArgument
 */
McpPromptArgument *mcp_prompt_argument_new (const gchar *name,
                                            const gchar *description,
                                            gboolean     required);

/**
 * mcp_prompt_argument_copy:
 * @arg: an #McpPromptArgument
 *
 * Creates a copy of the prompt argument.
 *
 * Returns: (transfer full): a copy of @arg
 */
McpPromptArgument *mcp_prompt_argument_copy (const McpPromptArgument *arg);

/**
 * mcp_prompt_argument_free:
 * @arg: an #McpPromptArgument
 *
 * Frees the prompt argument.
 */
void mcp_prompt_argument_free (McpPromptArgument *arg);

/**
 * mcp_prompt_argument_get_name:
 * @arg: an #McpPromptArgument
 *
 * Gets the name of the argument.
 *
 * Returns: (transfer none): the argument name
 */
const gchar *mcp_prompt_argument_get_name (const McpPromptArgument *arg);

/**
 * mcp_prompt_argument_get_description:
 * @arg: an #McpPromptArgument
 *
 * Gets the description of the argument.
 *
 * Returns: (transfer none) (nullable): the argument description
 */
const gchar *mcp_prompt_argument_get_description (const McpPromptArgument *arg);

/**
 * mcp_prompt_argument_get_required:
 * @arg: an #McpPromptArgument
 *
 * Gets whether the argument is required.
 *
 * Returns: %TRUE if the argument is required
 */
gboolean mcp_prompt_argument_get_required (const McpPromptArgument *arg);

/**
 * mcp_prompt_argument_to_json:
 * @arg: an #McpPromptArgument
 *
 * Serializes the argument to a JSON object.
 *
 * Returns: (transfer full): a #JsonNode containing the argument
 */
JsonNode *mcp_prompt_argument_to_json (const McpPromptArgument *arg);

/**
 * mcp_prompt_argument_new_from_json:
 * @node: a #JsonNode containing an argument definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new argument from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpPromptArgument, or %NULL on error
 */
McpPromptArgument *mcp_prompt_argument_new_from_json (JsonNode  *node,
                                                      GError   **error);

/* ========================================================================== */
/* McpPrompt                                                                  */
/* ========================================================================== */

#define MCP_TYPE_PROMPT (mcp_prompt_get_type ())

G_DECLARE_DERIVABLE_TYPE (McpPrompt, mcp_prompt, MCP, PROMPT, GObject)

/**
 * McpPromptClass:
 * @parent_class: the parent class
 *
 * The class structure for #McpPrompt.
 */
struct _McpPromptClass
{
    GObjectClass parent_class;

    /*< private >*/
    gpointer padding[8];
};

/**
 * mcp_prompt_new:
 * @name: the prompt name
 * @description: (nullable): human-readable description
 *
 * Creates a new prompt definition.
 *
 * Returns: (transfer full): a new #McpPrompt
 */
McpPrompt *mcp_prompt_new (const gchar *name,
                           const gchar *description);

/**
 * mcp_prompt_get_name:
 * @self: an #McpPrompt
 *
 * Gets the programmatic name of the prompt.
 *
 * Returns: (transfer none): the prompt name
 */
const gchar *mcp_prompt_get_name (McpPrompt *self);

/**
 * mcp_prompt_set_name:
 * @self: an #McpPrompt
 * @name: the prompt name
 *
 * Sets the programmatic name of the prompt.
 */
void mcp_prompt_set_name (McpPrompt   *self,
                          const gchar *name);

/**
 * mcp_prompt_get_title:
 * @self: an #McpPrompt
 *
 * Gets the human-readable title of the prompt.
 *
 * Returns: (transfer none) (nullable): the prompt title
 */
const gchar *mcp_prompt_get_title (McpPrompt *self);

/**
 * mcp_prompt_set_title:
 * @self: an #McpPrompt
 * @title: (nullable): the prompt title
 *
 * Sets the human-readable title of the prompt.
 */
void mcp_prompt_set_title (McpPrompt   *self,
                           const gchar *title);

/**
 * mcp_prompt_get_description:
 * @self: an #McpPrompt
 *
 * Gets the description of the prompt.
 *
 * Returns: (transfer none) (nullable): the prompt description
 */
const gchar *mcp_prompt_get_description (McpPrompt *self);

/**
 * mcp_prompt_set_description:
 * @self: an #McpPrompt
 * @description: (nullable): the prompt description
 *
 * Sets the description of the prompt.
 */
void mcp_prompt_set_description (McpPrompt   *self,
                                 const gchar *description);

/**
 * mcp_prompt_get_arguments:
 * @self: an #McpPrompt
 *
 * Gets the list of arguments for the prompt.
 *
 * Returns: (transfer none) (element-type McpPromptArgument) (nullable):
 *     the list of arguments, or %NULL if none
 */
GList *mcp_prompt_get_arguments (McpPrompt *self);

/**
 * mcp_prompt_add_argument:
 * @self: an #McpPrompt
 * @arg: (transfer full): an #McpPromptArgument to add
 *
 * Adds an argument to the prompt.
 */
void mcp_prompt_add_argument (McpPrompt          *self,
                              McpPromptArgument  *arg);

/**
 * mcp_prompt_add_argument_full:
 * @self: an #McpPrompt
 * @name: the argument name
 * @description: (nullable): the argument description
 * @required: whether the argument is required
 *
 * Convenience function to add an argument to the prompt.
 */
void mcp_prompt_add_argument_full (McpPrompt   *self,
                                   const gchar *name,
                                   const gchar *description,
                                   gboolean     required);

/**
 * mcp_prompt_clear_arguments:
 * @self: an #McpPrompt
 *
 * Removes all arguments from the prompt.
 */
void mcp_prompt_clear_arguments (McpPrompt *self);

/**
 * mcp_prompt_to_json:
 * @self: an #McpPrompt
 *
 * Serializes the prompt definition to a JSON object for MCP protocol.
 *
 * Returns: (transfer full): a #JsonNode containing the prompt definition
 */
JsonNode *mcp_prompt_to_json (McpPrompt *self);

/**
 * mcp_prompt_new_from_json:
 * @node: a #JsonNode containing a prompt definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new prompt from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpPrompt, or %NULL on error
 */
McpPrompt *mcp_prompt_new_from_json (JsonNode  *node,
                                     GError   **error);

G_END_DECLS

#endif /* MCP_PROMPT_H */
