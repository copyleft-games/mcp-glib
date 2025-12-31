/*
 * mcp-tool.c - Tool implementation for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp.h"

typedef struct
{
    gchar    *name;
    gchar    *title;
    gchar    *description;
    JsonNode *input_schema;
    JsonNode *output_schema;

    /* Annotation hints */
    gboolean  read_only_hint;
    gboolean  destructive_hint;
    gboolean  idempotent_hint;
    gboolean  open_world_hint;
} McpToolPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (McpTool, mcp_tool, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_NAME,
    PROP_TITLE,
    PROP_DESCRIPTION,
    PROP_INPUT_SCHEMA,
    PROP_OUTPUT_SCHEMA,
    PROP_READ_ONLY_HINT,
    PROP_DESTRUCTIVE_HINT,
    PROP_IDEMPOTENT_HINT,
    PROP_OPEN_WORLD_HINT,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
mcp_tool_finalize (GObject *object)
{
    McpTool *self = MCP_TOOL (object);
    McpToolPrivate *priv = mcp_tool_get_instance_private (self);

    g_clear_pointer (&priv->name, g_free);
    g_clear_pointer (&priv->title, g_free);
    g_clear_pointer (&priv->description, g_free);
    g_clear_pointer (&priv->input_schema, json_node_unref);
    g_clear_pointer (&priv->output_schema, json_node_unref);

    G_OBJECT_CLASS (mcp_tool_parent_class)->finalize (object);
}

static void
mcp_tool_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
    McpTool *self = MCP_TOOL (object);
    McpToolPrivate *priv = mcp_tool_get_instance_private (self);

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
    case PROP_INPUT_SCHEMA:
        g_value_set_boxed (value, priv->input_schema);
        break;
    case PROP_OUTPUT_SCHEMA:
        g_value_set_boxed (value, priv->output_schema);
        break;
    case PROP_READ_ONLY_HINT:
        g_value_set_boolean (value, priv->read_only_hint);
        break;
    case PROP_DESTRUCTIVE_HINT:
        g_value_set_boolean (value, priv->destructive_hint);
        break;
    case PROP_IDEMPOTENT_HINT:
        g_value_set_boolean (value, priv->idempotent_hint);
        break;
    case PROP_OPEN_WORLD_HINT:
        g_value_set_boolean (value, priv->open_world_hint);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mcp_tool_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    McpTool *self = MCP_TOOL (object);
    McpToolPrivate *priv = mcp_tool_get_instance_private (self);

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
    case PROP_INPUT_SCHEMA:
        g_clear_pointer (&priv->input_schema, json_node_unref);
        priv->input_schema = g_value_dup_boxed (value);
        break;
    case PROP_OUTPUT_SCHEMA:
        g_clear_pointer (&priv->output_schema, json_node_unref);
        priv->output_schema = g_value_dup_boxed (value);
        break;
    case PROP_READ_ONLY_HINT:
        priv->read_only_hint = g_value_get_boolean (value);
        break;
    case PROP_DESTRUCTIVE_HINT:
        priv->destructive_hint = g_value_get_boolean (value);
        break;
    case PROP_IDEMPOTENT_HINT:
        priv->idempotent_hint = g_value_get_boolean (value);
        break;
    case PROP_OPEN_WORLD_HINT:
        priv->open_world_hint = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mcp_tool_class_init (McpToolClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = mcp_tool_finalize;
    object_class->get_property = mcp_tool_get_property;
    object_class->set_property = mcp_tool_set_property;

    /**
     * McpTool:name:
     *
     * The programmatic name of the tool, used to identify it in tool calls.
     */
    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The programmatic name of the tool",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    /**
     * McpTool:title:
     *
     * Human-readable title for the tool, intended for UI display.
     */
    properties[PROP_TITLE] =
        g_param_spec_string ("title",
                             "Title",
                             "Human-readable title for the tool",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpTool:description:
     *
     * Human-readable description of what the tool does.
     */
    properties[PROP_DESCRIPTION] =
        g_param_spec_string ("description",
                             "Description",
                             "Human-readable description of the tool",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    /**
     * McpTool:input-schema:
     *
     * JSON Schema defining expected input parameters for the tool.
     */
    properties[PROP_INPUT_SCHEMA] =
        g_param_spec_boxed ("input-schema",
                            "Input Schema",
                            "JSON Schema for tool input parameters",
                            JSON_TYPE_NODE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpTool:output-schema:
     *
     * Optional JSON Schema defining the structure of the tool's output.
     */
    properties[PROP_OUTPUT_SCHEMA] =
        g_param_spec_boxed ("output-schema",
                            "Output Schema",
                            "JSON Schema for tool output structure",
                            JSON_TYPE_NODE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpTool:read-only-hint:
     *
     * Hint indicating whether the tool is read-only (does not modify environment).
     * Default: %FALSE
     */
    properties[PROP_READ_ONLY_HINT] =
        g_param_spec_boolean ("read-only-hint",
                              "Read Only Hint",
                              "Whether the tool is read-only",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpTool:destructive-hint:
     *
     * Hint indicating whether the tool may perform destructive updates.
     * Only meaningful when read-only-hint is %FALSE.
     * Default: %TRUE
     */
    properties[PROP_DESTRUCTIVE_HINT] =
        g_param_spec_boolean ("destructive-hint",
                              "Destructive Hint",
                              "Whether the tool may be destructive",
                              TRUE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpTool:idempotent-hint:
     *
     * Hint indicating whether calling the tool repeatedly with the same
     * arguments has no additional effect.
     * Default: %FALSE
     */
    properties[PROP_IDEMPOTENT_HINT] =
        g_param_spec_boolean ("idempotent-hint",
                              "Idempotent Hint",
                              "Whether the tool is idempotent",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpTool:open-world-hint:
     *
     * Hint indicating whether the tool may interact with external entities.
     * Default: %TRUE
     */
    properties[PROP_OPEN_WORLD_HINT] =
        g_param_spec_boolean ("open-world-hint",
                              "Open World Hint",
                              "Whether the tool interacts with external entities",
                              TRUE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mcp_tool_init (McpTool *self)
{
    McpToolPrivate *priv = mcp_tool_get_instance_private (self);

    /* Set defaults for hints */
    priv->read_only_hint = FALSE;
    priv->destructive_hint = TRUE;
    priv->idempotent_hint = FALSE;
    priv->open_world_hint = TRUE;
}

/**
 * mcp_tool_new:
 * @name: the tool name
 * @description: (nullable): human-readable description
 *
 * Creates a new tool definition.
 *
 * Returns: (transfer full): a new #McpTool
 */
McpTool *
mcp_tool_new (const gchar *name,
              const gchar *description)
{
    return g_object_new (MCP_TYPE_TOOL,
                         "name", name,
                         "description", description,
                         NULL);
}

/**
 * mcp_tool_get_name:
 * @self: an #McpTool
 *
 * Gets the programmatic name of the tool.
 *
 * Returns: (transfer none): the tool name
 */
const gchar *
mcp_tool_get_name (McpTool *self)
{
    McpToolPrivate *priv;

    g_return_val_if_fail (MCP_IS_TOOL (self), NULL);

    priv = mcp_tool_get_instance_private (self);
    return priv->name;
}

/**
 * mcp_tool_set_name:
 * @self: an #McpTool
 * @name: the tool name
 *
 * Sets the programmatic name of the tool.
 */
void
mcp_tool_set_name (McpTool     *self,
                   const gchar *name)
{
    g_return_if_fail (MCP_IS_TOOL (self));
    g_object_set (self, "name", name, NULL);
}

/**
 * mcp_tool_get_title:
 * @self: an #McpTool
 *
 * Gets the human-readable title of the tool.
 *
 * Returns: (transfer none) (nullable): the tool title
 */
const gchar *
mcp_tool_get_title (McpTool *self)
{
    McpToolPrivate *priv;

    g_return_val_if_fail (MCP_IS_TOOL (self), NULL);

    priv = mcp_tool_get_instance_private (self);
    return priv->title;
}

/**
 * mcp_tool_set_title:
 * @self: an #McpTool
 * @title: (nullable): the tool title
 *
 * Sets the human-readable title of the tool.
 */
void
mcp_tool_set_title (McpTool     *self,
                    const gchar *title)
{
    g_return_if_fail (MCP_IS_TOOL (self));
    g_object_set (self, "title", title, NULL);
}

/**
 * mcp_tool_get_description:
 * @self: an #McpTool
 *
 * Gets the description of the tool.
 *
 * Returns: (transfer none) (nullable): the tool description
 */
const gchar *
mcp_tool_get_description (McpTool *self)
{
    McpToolPrivate *priv;

    g_return_val_if_fail (MCP_IS_TOOL (self), NULL);

    priv = mcp_tool_get_instance_private (self);
    return priv->description;
}

/**
 * mcp_tool_set_description:
 * @self: an #McpTool
 * @description: (nullable): the tool description
 *
 * Sets the description of the tool.
 */
void
mcp_tool_set_description (McpTool     *self,
                          const gchar *description)
{
    g_return_if_fail (MCP_IS_TOOL (self));
    g_object_set (self, "description", description, NULL);
}

/**
 * mcp_tool_get_input_schema:
 * @self: an #McpTool
 *
 * Gets the JSON Schema defining expected input parameters.
 *
 * Returns: (transfer none) (nullable): the input schema
 */
JsonNode *
mcp_tool_get_input_schema (McpTool *self)
{
    McpToolPrivate *priv;

    g_return_val_if_fail (MCP_IS_TOOL (self), NULL);

    priv = mcp_tool_get_instance_private (self);
    return priv->input_schema;
}

/**
 * mcp_tool_set_input_schema:
 * @self: an #McpTool
 * @schema: (nullable): a #JsonNode containing the JSON Schema
 *
 * Sets the JSON Schema defining expected input parameters.
 */
void
mcp_tool_set_input_schema (McpTool  *self,
                           JsonNode *schema)
{
    g_return_if_fail (MCP_IS_TOOL (self));
    g_object_set (self, "input-schema", schema, NULL);
}

/**
 * mcp_tool_get_output_schema:
 * @self: an #McpTool
 *
 * Gets the JSON Schema defining the tool's structured output.
 *
 * Returns: (transfer none) (nullable): the output schema
 */
JsonNode *
mcp_tool_get_output_schema (McpTool *self)
{
    McpToolPrivate *priv;

    g_return_val_if_fail (MCP_IS_TOOL (self), NULL);

    priv = mcp_tool_get_instance_private (self);
    return priv->output_schema;
}

/**
 * mcp_tool_set_output_schema:
 * @self: an #McpTool
 * @schema: (nullable): a #JsonNode containing the JSON Schema
 *
 * Sets the JSON Schema defining the tool's structured output.
 */
void
mcp_tool_set_output_schema (McpTool  *self,
                            JsonNode *schema)
{
    g_return_if_fail (MCP_IS_TOOL (self));
    g_object_set (self, "output-schema", schema, NULL);
}

/**
 * mcp_tool_get_read_only_hint:
 * @self: an #McpTool
 *
 * Gets whether the tool is read-only.
 *
 * Returns: %TRUE if the tool is read-only
 */
gboolean
mcp_tool_get_read_only_hint (McpTool *self)
{
    McpToolPrivate *priv;

    g_return_val_if_fail (MCP_IS_TOOL (self), FALSE);

    priv = mcp_tool_get_instance_private (self);
    return priv->read_only_hint;
}

/**
 * mcp_tool_set_read_only_hint:
 * @self: an #McpTool
 * @read_only: whether the tool is read-only
 *
 * Sets whether the tool is read-only.
 */
void
mcp_tool_set_read_only_hint (McpTool  *self,
                             gboolean  read_only)
{
    g_return_if_fail (MCP_IS_TOOL (self));
    g_object_set (self, "read-only-hint", read_only, NULL);
}

/**
 * mcp_tool_get_destructive_hint:
 * @self: an #McpTool
 *
 * Gets whether the tool may be destructive.
 *
 * Returns: %TRUE if the tool may be destructive
 */
gboolean
mcp_tool_get_destructive_hint (McpTool *self)
{
    McpToolPrivate *priv;

    g_return_val_if_fail (MCP_IS_TOOL (self), TRUE);

    priv = mcp_tool_get_instance_private (self);
    return priv->destructive_hint;
}

/**
 * mcp_tool_set_destructive_hint:
 * @self: an #McpTool
 * @destructive: whether the tool may be destructive
 *
 * Sets whether the tool may be destructive.
 */
void
mcp_tool_set_destructive_hint (McpTool  *self,
                               gboolean  destructive)
{
    g_return_if_fail (MCP_IS_TOOL (self));
    g_object_set (self, "destructive-hint", destructive, NULL);
}

/**
 * mcp_tool_get_idempotent_hint:
 * @self: an #McpTool
 *
 * Gets whether the tool is idempotent.
 *
 * Returns: %TRUE if the tool is idempotent
 */
gboolean
mcp_tool_get_idempotent_hint (McpTool *self)
{
    McpToolPrivate *priv;

    g_return_val_if_fail (MCP_IS_TOOL (self), FALSE);

    priv = mcp_tool_get_instance_private (self);
    return priv->idempotent_hint;
}

/**
 * mcp_tool_set_idempotent_hint:
 * @self: an #McpTool
 * @idempotent: whether the tool is idempotent
 *
 * Sets whether the tool is idempotent.
 */
void
mcp_tool_set_idempotent_hint (McpTool  *self,
                              gboolean  idempotent)
{
    g_return_if_fail (MCP_IS_TOOL (self));
    g_object_set (self, "idempotent-hint", idempotent, NULL);
}

/**
 * mcp_tool_get_open_world_hint:
 * @self: an #McpTool
 *
 * Gets whether the tool has open world interaction.
 *
 * Returns: %TRUE if the tool has open world interaction
 */
gboolean
mcp_tool_get_open_world_hint (McpTool *self)
{
    McpToolPrivate *priv;

    g_return_val_if_fail (MCP_IS_TOOL (self), TRUE);

    priv = mcp_tool_get_instance_private (self);
    return priv->open_world_hint;
}

/**
 * mcp_tool_set_open_world_hint:
 * @self: an #McpTool
 * @open_world: whether the tool has open world interaction
 *
 * Sets whether the tool has open world interaction.
 */
void
mcp_tool_set_open_world_hint (McpTool  *self,
                              gboolean  open_world)
{
    g_return_if_fail (MCP_IS_TOOL (self));
    g_object_set (self, "open-world-hint", open_world, NULL);
}

/**
 * mcp_tool_to_json:
 * @self: an #McpTool
 *
 * Serializes the tool definition to a JSON object for MCP protocol.
 *
 * Returns: (transfer full): a #JsonNode containing the tool definition
 */
JsonNode *
mcp_tool_to_json (McpTool *self)
{
    McpToolPrivate *priv;
    JsonBuilder *builder;
    JsonNode *result;

    g_return_val_if_fail (MCP_IS_TOOL (self), NULL);

    priv = mcp_tool_get_instance_private (self);

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

    /* Required: inputSchema (empty object if not set) */
    json_builder_set_member_name (builder, "inputSchema");
    if (priv->input_schema != NULL)
    {
        json_builder_add_value (builder, json_node_copy (priv->input_schema));
    }
    else
    {
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "type");
        json_builder_add_string_value (builder, "object");
        json_builder_end_object (builder);
    }

    /* Optional: outputSchema */
    if (priv->output_schema != NULL)
    {
        json_builder_set_member_name (builder, "outputSchema");
        json_builder_add_value (builder, json_node_copy (priv->output_schema));
    }

    /* Optional: annotations (only include if non-default) */
    if (priv->read_only_hint || !priv->destructive_hint ||
        priv->idempotent_hint || !priv->open_world_hint || priv->title != NULL)
    {
        json_builder_set_member_name (builder, "annotations");
        json_builder_begin_object (builder);

        if (priv->title != NULL)
        {
            json_builder_set_member_name (builder, "title");
            json_builder_add_string_value (builder, priv->title);
        }

        if (priv->read_only_hint)
        {
            json_builder_set_member_name (builder, "readOnlyHint");
            json_builder_add_boolean_value (builder, TRUE);
        }

        if (!priv->destructive_hint)
        {
            json_builder_set_member_name (builder, "destructiveHint");
            json_builder_add_boolean_value (builder, FALSE);
        }

        if (priv->idempotent_hint)
        {
            json_builder_set_member_name (builder, "idempotentHint");
            json_builder_add_boolean_value (builder, TRUE);
        }

        if (!priv->open_world_hint)
        {
            json_builder_set_member_name (builder, "openWorldHint");
            json_builder_add_boolean_value (builder, FALSE);
        }

        json_builder_end_object (builder);
    }

    json_builder_end_object (builder);

    result = json_builder_get_root (builder);
    g_object_unref (builder);

    return result;
}

/**
 * mcp_tool_new_from_json:
 * @node: a #JsonNode containing a tool definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new tool from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpTool, or %NULL on error
 */
McpTool *
mcp_tool_new_from_json (JsonNode  *node,
                        GError   **error)
{
    JsonObject *obj;
    McpTool *tool;
    const gchar *name;
    const gchar *title = NULL;
    const gchar *description = NULL;
    JsonNode *input_schema = NULL;
    JsonNode *output_schema = NULL;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Tool definition must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    /* Required: name */
    if (!json_object_has_member (obj, "name"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Tool definition missing required 'name' field");
        return NULL;
    }
    name = json_object_get_string_member (obj, "name");

    /* Optional fields */
    if (json_object_has_member (obj, "title"))
        title = json_object_get_string_member (obj, "title");

    if (json_object_has_member (obj, "description"))
        description = json_object_get_string_member (obj, "description");

    if (json_object_has_member (obj, "inputSchema"))
        input_schema = json_object_get_member (obj, "inputSchema");

    if (json_object_has_member (obj, "outputSchema"))
        output_schema = json_object_get_member (obj, "outputSchema");

    tool = mcp_tool_new (name, description);

    if (title != NULL)
        mcp_tool_set_title (tool, title);

    if (input_schema != NULL)
        mcp_tool_set_input_schema (tool, input_schema);

    if (output_schema != NULL)
        mcp_tool_set_output_schema (tool, output_schema);

    /* Parse annotations if present */
    if (json_object_has_member (obj, "annotations"))
    {
        JsonObject *annotations = json_object_get_object_member (obj, "annotations");

        if (json_object_has_member (annotations, "title"))
            mcp_tool_set_title (tool, json_object_get_string_member (annotations, "title"));

        if (json_object_has_member (annotations, "readOnlyHint"))
            mcp_tool_set_read_only_hint (tool, json_object_get_boolean_member (annotations, "readOnlyHint"));

        if (json_object_has_member (annotations, "destructiveHint"))
            mcp_tool_set_destructive_hint (tool, json_object_get_boolean_member (annotations, "destructiveHint"));

        if (json_object_has_member (annotations, "idempotentHint"))
            mcp_tool_set_idempotent_hint (tool, json_object_get_boolean_member (annotations, "idempotentHint"));

        if (json_object_has_member (annotations, "openWorldHint"))
            mcp_tool_set_open_world_hint (tool, json_object_get_boolean_member (annotations, "openWorldHint"));
    }

    return tool;
}
