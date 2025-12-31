/*
 * mcp-prompt.c - Prompt implementation for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp.h"

/* ========================================================================== */
/* McpPromptArgument - Boxed type                                             */
/* ========================================================================== */

struct _McpPromptArgument
{
    gchar    *name;
    gchar    *description;
    gboolean  required;
};

G_DEFINE_BOXED_TYPE (McpPromptArgument,
                     mcp_prompt_argument,
                     mcp_prompt_argument_copy,
                     mcp_prompt_argument_free)

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
McpPromptArgument *
mcp_prompt_argument_new (const gchar *name,
                         const gchar *description,
                         gboolean     required)
{
    McpPromptArgument *arg;

    arg = g_slice_new0 (McpPromptArgument);
    arg->name = g_strdup (name);
    arg->description = g_strdup (description);
    arg->required = required;

    return arg;
}

/**
 * mcp_prompt_argument_copy:
 * @arg: an #McpPromptArgument
 *
 * Creates a copy of the prompt argument.
 *
 * Returns: (transfer full): a copy of @arg
 */
McpPromptArgument *
mcp_prompt_argument_copy (const McpPromptArgument *arg)
{
    if (arg == NULL)
        return NULL;

    return mcp_prompt_argument_new (arg->name, arg->description, arg->required);
}

/**
 * mcp_prompt_argument_free:
 * @arg: an #McpPromptArgument
 *
 * Frees the prompt argument.
 */
void
mcp_prompt_argument_free (McpPromptArgument *arg)
{
    if (arg == NULL)
        return;

    g_free (arg->name);
    g_free (arg->description);
    g_slice_free (McpPromptArgument, arg);
}

/**
 * mcp_prompt_argument_get_name:
 * @arg: an #McpPromptArgument
 *
 * Gets the name of the argument.
 *
 * Returns: (transfer none): the argument name
 */
const gchar *
mcp_prompt_argument_get_name (const McpPromptArgument *arg)
{
    g_return_val_if_fail (arg != NULL, NULL);
    return arg->name;
}

/**
 * mcp_prompt_argument_get_description:
 * @arg: an #McpPromptArgument
 *
 * Gets the description of the argument.
 *
 * Returns: (transfer none) (nullable): the argument description
 */
const gchar *
mcp_prompt_argument_get_description (const McpPromptArgument *arg)
{
    g_return_val_if_fail (arg != NULL, NULL);
    return arg->description;
}

/**
 * mcp_prompt_argument_get_required:
 * @arg: an #McpPromptArgument
 *
 * Gets whether the argument is required.
 *
 * Returns: %TRUE if the argument is required
 */
gboolean
mcp_prompt_argument_get_required (const McpPromptArgument *arg)
{
    g_return_val_if_fail (arg != NULL, FALSE);
    return arg->required;
}

/**
 * mcp_prompt_argument_to_json:
 * @arg: an #McpPromptArgument
 *
 * Serializes the argument to a JSON object.
 *
 * Returns: (transfer full): a #JsonNode containing the argument
 */
JsonNode *
mcp_prompt_argument_to_json (const McpPromptArgument *arg)
{
    JsonBuilder *builder;
    JsonNode *result;

    g_return_val_if_fail (arg != NULL, NULL);

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    /* Required: name */
    json_builder_set_member_name (builder, "name");
    json_builder_add_string_value (builder, arg->name ? arg->name : "");

    /* Optional: description */
    if (arg->description != NULL)
    {
        json_builder_set_member_name (builder, "description");
        json_builder_add_string_value (builder, arg->description);
    }

    /* Optional: required (only include if true) */
    if (arg->required)
    {
        json_builder_set_member_name (builder, "required");
        json_builder_add_boolean_value (builder, TRUE);
    }

    json_builder_end_object (builder);

    result = json_builder_get_root (builder);
    g_object_unref (builder);

    return result;
}

/**
 * mcp_prompt_argument_new_from_json:
 * @node: a #JsonNode containing an argument definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new argument from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpPromptArgument, or %NULL on error
 */
McpPromptArgument *
mcp_prompt_argument_new_from_json (JsonNode  *node,
                                   GError   **error)
{
    JsonObject *obj;
    const gchar *name;
    const gchar *description = NULL;
    gboolean required = FALSE;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Prompt argument must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    /* Required: name */
    if (!json_object_has_member (obj, "name"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Prompt argument missing required 'name' field");
        return NULL;
    }
    name = json_object_get_string_member (obj, "name");

    /* Optional fields */
    if (json_object_has_member (obj, "description"))
        description = json_object_get_string_member (obj, "description");

    if (json_object_has_member (obj, "required"))
        required = json_object_get_boolean_member (obj, "required");

    return mcp_prompt_argument_new (name, description, required);
}

/* ========================================================================== */
/* McpPrompt                                                                  */
/* ========================================================================== */

typedef struct
{
    gchar *name;
    gchar *title;
    gchar *description;
    GList *arguments;
} McpPromptPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (McpPrompt, mcp_prompt, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_NAME,
    PROP_TITLE,
    PROP_DESCRIPTION,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
mcp_prompt_finalize (GObject *object)
{
    McpPrompt *self = MCP_PROMPT (object);
    McpPromptPrivate *priv = mcp_prompt_get_instance_private (self);

    g_clear_pointer (&priv->name, g_free);
    g_clear_pointer (&priv->title, g_free);
    g_clear_pointer (&priv->description, g_free);
    g_list_free_full (priv->arguments, (GDestroyNotify) mcp_prompt_argument_free);

    G_OBJECT_CLASS (mcp_prompt_parent_class)->finalize (object);
}

static void
mcp_prompt_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    McpPrompt *self = MCP_PROMPT (object);
    McpPromptPrivate *priv = mcp_prompt_get_instance_private (self);

    switch (prop_id)
    {
    case PROP_NAME:
        g_value_set_string (value, priv->name);
        break;
    case PROP_TITLE:
        g_value_set_string (value, priv->title);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, priv->description);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mcp_prompt_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    McpPrompt *self = MCP_PROMPT (object);
    McpPromptPrivate *priv = mcp_prompt_get_instance_private (self);

    switch (prop_id)
    {
    case PROP_NAME:
        g_free (priv->name);
        priv->name = g_value_dup_string (value);
        break;
    case PROP_TITLE:
        g_free (priv->title);
        priv->title = g_value_dup_string (value);
        break;
    case PROP_DESCRIPTION:
        g_free (priv->description);
        priv->description = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mcp_prompt_class_init (McpPromptClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = mcp_prompt_finalize;
    object_class->get_property = mcp_prompt_get_property;
    object_class->set_property = mcp_prompt_set_property;

    /**
     * McpPrompt:name:
     *
     * The programmatic name of the prompt.
     */
    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The programmatic name of the prompt",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    /**
     * McpPrompt:title:
     *
     * Human-readable title for the prompt, intended for UI display.
     */
    properties[PROP_TITLE] =
        g_param_spec_string ("title",
                             "Title",
                             "Human-readable title for the prompt",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpPrompt:description:
     *
     * Human-readable description of what the prompt provides.
     */
    properties[PROP_DESCRIPTION] =
        g_param_spec_string ("description",
                             "Description",
                             "Human-readable description of the prompt",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mcp_prompt_init (McpPrompt *self)
{
    /* No default initialization needed */
}

/**
 * mcp_prompt_new:
 * @name: the prompt name
 * @description: (nullable): human-readable description
 *
 * Creates a new prompt definition.
 *
 * Returns: (transfer full): a new #McpPrompt
 */
McpPrompt *
mcp_prompt_new (const gchar *name,
                const gchar *description)
{
    return g_object_new (MCP_TYPE_PROMPT,
                         "name", name,
                         "description", description,
                         NULL);
}

/**
 * mcp_prompt_get_name:
 * @self: an #McpPrompt
 *
 * Gets the programmatic name of the prompt.
 *
 * Returns: (transfer none): the prompt name
 */
const gchar *
mcp_prompt_get_name (McpPrompt *self)
{
    McpPromptPrivate *priv;

    g_return_val_if_fail (MCP_IS_PROMPT (self), NULL);

    priv = mcp_prompt_get_instance_private (self);
    return priv->name;
}

/**
 * mcp_prompt_set_name:
 * @self: an #McpPrompt
 * @name: the prompt name
 *
 * Sets the programmatic name of the prompt.
 */
void
mcp_prompt_set_name (McpPrompt   *self,
                     const gchar *name)
{
    g_return_if_fail (MCP_IS_PROMPT (self));
    g_object_set (self, "name", name, NULL);
}

/**
 * mcp_prompt_get_title:
 * @self: an #McpPrompt
 *
 * Gets the human-readable title of the prompt.
 *
 * Returns: (transfer none) (nullable): the prompt title
 */
const gchar *
mcp_prompt_get_title (McpPrompt *self)
{
    McpPromptPrivate *priv;

    g_return_val_if_fail (MCP_IS_PROMPT (self), NULL);

    priv = mcp_prompt_get_instance_private (self);
    return priv->title;
}

/**
 * mcp_prompt_set_title:
 * @self: an #McpPrompt
 * @title: (nullable): the prompt title
 *
 * Sets the human-readable title of the prompt.
 */
void
mcp_prompt_set_title (McpPrompt   *self,
                      const gchar *title)
{
    g_return_if_fail (MCP_IS_PROMPT (self));
    g_object_set (self, "title", title, NULL);
}

/**
 * mcp_prompt_get_description:
 * @self: an #McpPrompt
 *
 * Gets the description of the prompt.
 *
 * Returns: (transfer none) (nullable): the prompt description
 */
const gchar *
mcp_prompt_get_description (McpPrompt *self)
{
    McpPromptPrivate *priv;

    g_return_val_if_fail (MCP_IS_PROMPT (self), NULL);

    priv = mcp_prompt_get_instance_private (self);
    return priv->description;
}

/**
 * mcp_prompt_set_description:
 * @self: an #McpPrompt
 * @description: (nullable): the prompt description
 *
 * Sets the description of the prompt.
 */
void
mcp_prompt_set_description (McpPrompt   *self,
                            const gchar *description)
{
    g_return_if_fail (MCP_IS_PROMPT (self));
    g_object_set (self, "description", description, NULL);
}

/**
 * mcp_prompt_get_arguments:
 * @self: an #McpPrompt
 *
 * Gets the list of arguments for the prompt.
 *
 * Returns: (transfer none) (element-type McpPromptArgument) (nullable):
 *     the list of arguments, or %NULL if none
 */
GList *
mcp_prompt_get_arguments (McpPrompt *self)
{
    McpPromptPrivate *priv;

    g_return_val_if_fail (MCP_IS_PROMPT (self), NULL);

    priv = mcp_prompt_get_instance_private (self);
    return priv->arguments;
}

/**
 * mcp_prompt_add_argument:
 * @self: an #McpPrompt
 * @arg: (transfer full): an #McpPromptArgument to add
 *
 * Adds an argument to the prompt.
 */
void
mcp_prompt_add_argument (McpPrompt          *self,
                         McpPromptArgument  *arg)
{
    McpPromptPrivate *priv;

    g_return_if_fail (MCP_IS_PROMPT (self));
    g_return_if_fail (arg != NULL);

    priv = mcp_prompt_get_instance_private (self);
    priv->arguments = g_list_append (priv->arguments, arg);
}

/**
 * mcp_prompt_add_argument_full:
 * @self: an #McpPrompt
 * @name: the argument name
 * @description: (nullable): the argument description
 * @required: whether the argument is required
 *
 * Convenience function to add an argument to the prompt.
 */
void
mcp_prompt_add_argument_full (McpPrompt   *self,
                              const gchar *name,
                              const gchar *description,
                              gboolean     required)
{
    McpPromptArgument *arg;

    g_return_if_fail (MCP_IS_PROMPT (self));
    g_return_if_fail (name != NULL);

    arg = mcp_prompt_argument_new (name, description, required);
    mcp_prompt_add_argument (self, arg);
}

/**
 * mcp_prompt_clear_arguments:
 * @self: an #McpPrompt
 *
 * Removes all arguments from the prompt.
 */
void
mcp_prompt_clear_arguments (McpPrompt *self)
{
    McpPromptPrivate *priv;

    g_return_if_fail (MCP_IS_PROMPT (self));

    priv = mcp_prompt_get_instance_private (self);
    g_list_free_full (priv->arguments, (GDestroyNotify) mcp_prompt_argument_free);
    priv->arguments = NULL;
}

/**
 * mcp_prompt_to_json:
 * @self: an #McpPrompt
 *
 * Serializes the prompt definition to a JSON object for MCP protocol.
 *
 * Returns: (transfer full): a #JsonNode containing the prompt definition
 */
JsonNode *
mcp_prompt_to_json (McpPrompt *self)
{
    McpPromptPrivate *priv;
    JsonBuilder *builder;
    JsonNode *result;
    GList *iter;

    g_return_val_if_fail (MCP_IS_PROMPT (self), NULL);

    priv = mcp_prompt_get_instance_private (self);

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    /* Required: name */
    json_builder_set_member_name (builder, "name");
    json_builder_add_string_value (builder, priv->name ? priv->name : "");

    /* Optional: title */
    if (priv->title != NULL)
    {
        json_builder_set_member_name (builder, "title");
        json_builder_add_string_value (builder, priv->title);
    }

    /* Optional: description */
    if (priv->description != NULL)
    {
        json_builder_set_member_name (builder, "description");
        json_builder_add_string_value (builder, priv->description);
    }

    /* Optional: arguments */
    if (priv->arguments != NULL)
    {
        json_builder_set_member_name (builder, "arguments");
        json_builder_begin_array (builder);

        for (iter = priv->arguments; iter != NULL; iter = iter->next)
        {
            McpPromptArgument *arg = (McpPromptArgument *) iter->data;
            JsonNode *arg_node = mcp_prompt_argument_to_json (arg);
            json_builder_add_value (builder, arg_node);
        }

        json_builder_end_array (builder);
    }

    json_builder_end_object (builder);

    result = json_builder_get_root (builder);
    g_object_unref (builder);

    return result;
}

/**
 * mcp_prompt_new_from_json:
 * @node: a #JsonNode containing a prompt definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new prompt from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpPrompt, or %NULL on error
 */
McpPrompt *
mcp_prompt_new_from_json (JsonNode  *node,
                          GError   **error)
{
    JsonObject *obj;
    McpPrompt *prompt;
    const gchar *name;
    const gchar *title = NULL;
    const gchar *description = NULL;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Prompt definition must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    /* Required: name */
    if (!json_object_has_member (obj, "name"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Prompt definition missing required 'name' field");
        return NULL;
    }
    name = json_object_get_string_member (obj, "name");

    /* Optional fields */
    if (json_object_has_member (obj, "title"))
        title = json_object_get_string_member (obj, "title");

    if (json_object_has_member (obj, "description"))
        description = json_object_get_string_member (obj, "description");

    prompt = mcp_prompt_new (name, description);

    if (title != NULL)
        mcp_prompt_set_title (prompt, title);

    /* Parse arguments if present */
    if (json_object_has_member (obj, "arguments"))
    {
        JsonArray *args_array = json_object_get_array_member (obj, "arguments");
        guint i, len = json_array_get_length (args_array);

        for (i = 0; i < len; i++)
        {
            JsonNode *arg_node = json_array_get_element (args_array, i);
            g_autoptr(GError) arg_error = NULL;
            McpPromptArgument *arg;

            arg = mcp_prompt_argument_new_from_json (arg_node, &arg_error);
            if (arg == NULL)
            {
                g_object_unref (prompt);
                g_propagate_error (error, g_steal_pointer (&arg_error));
                return NULL;
            }

            mcp_prompt_add_argument (prompt, arg);
        }
    }

    return prompt;
}
