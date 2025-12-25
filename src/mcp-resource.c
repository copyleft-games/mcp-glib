/*
 * mcp-resource.c - Resource implementation for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define MCP_COMPILATION
#include <mcp/mcp.h>

/* ========================================================================== */
/* McpResource                                                                */
/* ========================================================================== */

typedef struct
{
    gchar *uri;
    gchar *name;
    gchar *title;
    gchar *description;
    gchar *mime_type;
    gint64 size;
} McpResourcePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (McpResource, mcp_resource, G_TYPE_OBJECT)

enum
{
    RESOURCE_PROP_0,
    RESOURCE_PROP_URI,
    RESOURCE_PROP_NAME,
    RESOURCE_PROP_TITLE,
    RESOURCE_PROP_DESCRIPTION,
    RESOURCE_PROP_MIME_TYPE,
    RESOURCE_PROP_SIZE,
    RESOURCE_N_PROPS
};

static GParamSpec *resource_properties[RESOURCE_N_PROPS];

static void
mcp_resource_finalize (GObject *object)
{
    McpResource *self = MCP_RESOURCE (object);
    McpResourcePrivate *priv = mcp_resource_get_instance_private (self);

    g_clear_pointer (&priv->uri, g_free);
    g_clear_pointer (&priv->name, g_free);
    g_clear_pointer (&priv->title, g_free);
    g_clear_pointer (&priv->description, g_free);
    g_clear_pointer (&priv->mime_type, g_free);

    G_OBJECT_CLASS (mcp_resource_parent_class)->finalize (object);
}

static void
mcp_resource_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    McpResource *self = MCP_RESOURCE (object);
    McpResourcePrivate *priv = mcp_resource_get_instance_private (self);

    switch (prop_id)
    {
    case RESOURCE_PROP_URI:
        g_value_set_string (value, priv->uri);
        break;
    case RESOURCE_PROP_NAME:
        g_value_set_string (value, priv->name);
        break;
    case RESOURCE_PROP_TITLE:
        g_value_set_string (value, priv->title);
        break;
    case RESOURCE_PROP_DESCRIPTION:
        g_value_set_string (value, priv->description);
        break;
    case RESOURCE_PROP_MIME_TYPE:
        g_value_set_string (value, priv->mime_type);
        break;
    case RESOURCE_PROP_SIZE:
        g_value_set_int64 (value, priv->size);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mcp_resource_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    McpResource *self = MCP_RESOURCE (object);
    McpResourcePrivate *priv = mcp_resource_get_instance_private (self);

    switch (prop_id)
    {
    case RESOURCE_PROP_URI:
        g_free (priv->uri);
        priv->uri = g_value_dup_string (value);
        break;
    case RESOURCE_PROP_NAME:
        g_free (priv->name);
        priv->name = g_value_dup_string (value);
        break;
    case RESOURCE_PROP_TITLE:
        g_free (priv->title);
        priv->title = g_value_dup_string (value);
        break;
    case RESOURCE_PROP_DESCRIPTION:
        g_free (priv->description);
        priv->description = g_value_dup_string (value);
        break;
    case RESOURCE_PROP_MIME_TYPE:
        g_free (priv->mime_type);
        priv->mime_type = g_value_dup_string (value);
        break;
    case RESOURCE_PROP_SIZE:
        priv->size = g_value_get_int64 (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mcp_resource_class_init (McpResourceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = mcp_resource_finalize;
    object_class->get_property = mcp_resource_get_property;
    object_class->set_property = mcp_resource_set_property;

    /**
     * McpResource:uri:
     *
     * The URI of the resource.
     */
    resource_properties[RESOURCE_PROP_URI] =
        g_param_spec_string ("uri",
                             "URI",
                             "The URI of the resource",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    /**
     * McpResource:name:
     *
     * The programmatic name of the resource.
     */
    resource_properties[RESOURCE_PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The programmatic name of the resource",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    /**
     * McpResource:title:
     *
     * Human-readable title for the resource, intended for UI display.
     */
    resource_properties[RESOURCE_PROP_TITLE] =
        g_param_spec_string ("title",
                             "Title",
                             "Human-readable title for the resource",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpResource:description:
     *
     * Human-readable description of what the resource represents.
     */
    resource_properties[RESOURCE_PROP_DESCRIPTION] =
        g_param_spec_string ("description",
                             "Description",
                             "Human-readable description of the resource",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpResource:mime-type:
     *
     * The MIME type of the resource content.
     */
    resource_properties[RESOURCE_PROP_MIME_TYPE] =
        g_param_spec_string ("mime-type",
                             "MIME Type",
                             "The MIME type of the resource content",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpResource:size:
     *
     * The size of the raw resource content in bytes.
     * -1 indicates the size is unknown.
     */
    resource_properties[RESOURCE_PROP_SIZE] =
        g_param_spec_int64 ("size",
                            "Size",
                            "The size of the resource content in bytes",
                            -1, G_MAXINT64, -1,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, RESOURCE_N_PROPS, resource_properties);
}

static void
mcp_resource_init (McpResource *self)
{
    McpResourcePrivate *priv = mcp_resource_get_instance_private (self);

    priv->size = -1;
}

/**
 * mcp_resource_new:
 * @uri: the resource URI
 * @name: the resource name
 *
 * Creates a new resource definition.
 *
 * Returns: (transfer full): a new #McpResource
 */
McpResource *
mcp_resource_new (const gchar *uri,
                  const gchar *name)
{
    return g_object_new (MCP_TYPE_RESOURCE,
                         "uri", uri,
                         "name", name,
                         NULL);
}

/**
 * mcp_resource_get_uri:
 * @self: an #McpResource
 *
 * Gets the URI of the resource.
 *
 * Returns: (transfer none): the resource URI
 */
const gchar *
mcp_resource_get_uri (McpResource *self)
{
    McpResourcePrivate *priv;

    g_return_val_if_fail (MCP_IS_RESOURCE (self), NULL);

    priv = mcp_resource_get_instance_private (self);
    return priv->uri;
}

/**
 * mcp_resource_set_uri:
 * @self: an #McpResource
 * @uri: the resource URI
 *
 * Sets the URI of the resource.
 */
void
mcp_resource_set_uri (McpResource *self,
                      const gchar *uri)
{
    g_return_if_fail (MCP_IS_RESOURCE (self));
    g_object_set (self, "uri", uri, NULL);
}

/**
 * mcp_resource_get_name:
 * @self: an #McpResource
 *
 * Gets the programmatic name of the resource.
 *
 * Returns: (transfer none): the resource name
 */
const gchar *
mcp_resource_get_name (McpResource *self)
{
    McpResourcePrivate *priv;

    g_return_val_if_fail (MCP_IS_RESOURCE (self), NULL);

    priv = mcp_resource_get_instance_private (self);
    return priv->name;
}

/**
 * mcp_resource_set_name:
 * @self: an #McpResource
 * @name: the resource name
 *
 * Sets the programmatic name of the resource.
 */
void
mcp_resource_set_name (McpResource *self,
                       const gchar *name)
{
    g_return_if_fail (MCP_IS_RESOURCE (self));
    g_object_set (self, "name", name, NULL);
}

/**
 * mcp_resource_get_title:
 * @self: an #McpResource
 *
 * Gets the human-readable title of the resource.
 *
 * Returns: (transfer none) (nullable): the resource title
 */
const gchar *
mcp_resource_get_title (McpResource *self)
{
    McpResourcePrivate *priv;

    g_return_val_if_fail (MCP_IS_RESOURCE (self), NULL);

    priv = mcp_resource_get_instance_private (self);
    return priv->title;
}

/**
 * mcp_resource_set_title:
 * @self: an #McpResource
 * @title: (nullable): the resource title
 *
 * Sets the human-readable title of the resource.
 */
void
mcp_resource_set_title (McpResource *self,
                        const gchar *title)
{
    g_return_if_fail (MCP_IS_RESOURCE (self));
    g_object_set (self, "title", title, NULL);
}

/**
 * mcp_resource_get_description:
 * @self: an #McpResource
 *
 * Gets the description of the resource.
 *
 * Returns: (transfer none) (nullable): the resource description
 */
const gchar *
mcp_resource_get_description (McpResource *self)
{
    McpResourcePrivate *priv;

    g_return_val_if_fail (MCP_IS_RESOURCE (self), NULL);

    priv = mcp_resource_get_instance_private (self);
    return priv->description;
}

/**
 * mcp_resource_set_description:
 * @self: an #McpResource
 * @description: (nullable): the resource description
 *
 * Sets the description of the resource.
 */
void
mcp_resource_set_description (McpResource *self,
                              const gchar *description)
{
    g_return_if_fail (MCP_IS_RESOURCE (self));
    g_object_set (self, "description", description, NULL);
}

/**
 * mcp_resource_get_mime_type:
 * @self: an #McpResource
 *
 * Gets the MIME type of the resource.
 *
 * Returns: (transfer none) (nullable): the MIME type
 */
const gchar *
mcp_resource_get_mime_type (McpResource *self)
{
    McpResourcePrivate *priv;

    g_return_val_if_fail (MCP_IS_RESOURCE (self), NULL);

    priv = mcp_resource_get_instance_private (self);
    return priv->mime_type;
}

/**
 * mcp_resource_set_mime_type:
 * @self: an #McpResource
 * @mime_type: (nullable): the MIME type
 *
 * Sets the MIME type of the resource.
 */
void
mcp_resource_set_mime_type (McpResource *self,
                            const gchar *mime_type)
{
    g_return_if_fail (MCP_IS_RESOURCE (self));
    g_object_set (self, "mime-type", mime_type, NULL);
}

/**
 * mcp_resource_get_size:
 * @self: an #McpResource
 *
 * Gets the size of the resource content in bytes.
 *
 * Returns: the size in bytes, or -1 if unknown
 */
gint64
mcp_resource_get_size (McpResource *self)
{
    McpResourcePrivate *priv;

    g_return_val_if_fail (MCP_IS_RESOURCE (self), -1);

    priv = mcp_resource_get_instance_private (self);
    return priv->size;
}

/**
 * mcp_resource_set_size:
 * @self: an #McpResource
 * @size: the size in bytes, or -1 if unknown
 *
 * Sets the size of the resource content in bytes.
 */
void
mcp_resource_set_size (McpResource *self,
                       gint64       size)
{
    g_return_if_fail (MCP_IS_RESOURCE (self));
    g_object_set (self, "size", size, NULL);
}

/**
 * mcp_resource_to_json:
 * @self: an #McpResource
 *
 * Serializes the resource definition to a JSON object for MCP protocol.
 *
 * Returns: (transfer full): a #JsonNode containing the resource definition
 */
JsonNode *
mcp_resource_to_json (McpResource *self)
{
    McpResourcePrivate *priv;
    JsonBuilder *builder;
    JsonNode *result;

    g_return_val_if_fail (MCP_IS_RESOURCE (self), NULL);

    priv = mcp_resource_get_instance_private (self);

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    /* Required: uri */
    json_builder_set_member_name (builder, "uri");
    json_builder_add_string_value (builder, priv->uri ? priv->uri : "");

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

    /* Optional: mimeType */
    if (priv->mime_type != NULL)
    {
        json_builder_set_member_name (builder, "mimeType");
        json_builder_add_string_value (builder, priv->mime_type);
    }

    /* Optional: size (only include if known) */
    if (priv->size >= 0)
    {
        json_builder_set_member_name (builder, "size");
        json_builder_add_int_value (builder, priv->size);
    }

    json_builder_end_object (builder);

    result = json_builder_get_root (builder);
    g_object_unref (builder);

    return result;
}

/**
 * mcp_resource_new_from_json:
 * @node: a #JsonNode containing a resource definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new resource from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpResource, or %NULL on error
 */
McpResource *
mcp_resource_new_from_json (JsonNode  *node,
                            GError   **error)
{
    JsonObject *obj;
    McpResource *resource;
    const gchar *uri;
    const gchar *name;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Resource definition must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    /* Required: uri */
    if (!json_object_has_member (obj, "uri"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Resource definition missing required 'uri' field");
        return NULL;
    }
    uri = json_object_get_string_member (obj, "uri");

    /* Required: name */
    if (!json_object_has_member (obj, "name"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Resource definition missing required 'name' field");
        return NULL;
    }
    name = json_object_get_string_member (obj, "name");

    resource = mcp_resource_new (uri, name);

    /* Optional fields */
    if (json_object_has_member (obj, "title"))
        mcp_resource_set_title (resource, json_object_get_string_member (obj, "title"));

    if (json_object_has_member (obj, "description"))
        mcp_resource_set_description (resource, json_object_get_string_member (obj, "description"));

    if (json_object_has_member (obj, "mimeType"))
        mcp_resource_set_mime_type (resource, json_object_get_string_member (obj, "mimeType"));

    if (json_object_has_member (obj, "size"))
        mcp_resource_set_size (resource, json_object_get_int_member (obj, "size"));

    return resource;
}

/* ========================================================================== */
/* McpResourceTemplate                                                        */
/* ========================================================================== */

typedef struct
{
    gchar *uri_template;
    gchar *name;
    gchar *title;
    gchar *description;
    gchar *mime_type;
} McpResourceTemplatePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (McpResourceTemplate, mcp_resource_template, G_TYPE_OBJECT)

enum
{
    TEMPLATE_PROP_0,
    TEMPLATE_PROP_URI_TEMPLATE,
    TEMPLATE_PROP_NAME,
    TEMPLATE_PROP_TITLE,
    TEMPLATE_PROP_DESCRIPTION,
    TEMPLATE_PROP_MIME_TYPE,
    TEMPLATE_N_PROPS
};

static GParamSpec *template_properties[TEMPLATE_N_PROPS];

static void
mcp_resource_template_finalize (GObject *object)
{
    McpResourceTemplate *self = MCP_RESOURCE_TEMPLATE (object);
    McpResourceTemplatePrivate *priv = mcp_resource_template_get_instance_private (self);

    g_clear_pointer (&priv->uri_template, g_free);
    g_clear_pointer (&priv->name, g_free);
    g_clear_pointer (&priv->title, g_free);
    g_clear_pointer (&priv->description, g_free);
    g_clear_pointer (&priv->mime_type, g_free);

    G_OBJECT_CLASS (mcp_resource_template_parent_class)->finalize (object);
}

static void
mcp_resource_template_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    McpResourceTemplate *self = MCP_RESOURCE_TEMPLATE (object);
    McpResourceTemplatePrivate *priv = mcp_resource_template_get_instance_private (self);

    switch (prop_id)
    {
    case TEMPLATE_PROP_URI_TEMPLATE:
        g_value_set_string (value, priv->uri_template);
        break;
    case TEMPLATE_PROP_NAME:
        g_value_set_string (value, priv->name);
        break;
    case TEMPLATE_PROP_TITLE:
        g_value_set_string (value, priv->title);
        break;
    case TEMPLATE_PROP_DESCRIPTION:
        g_value_set_string (value, priv->description);
        break;
    case TEMPLATE_PROP_MIME_TYPE:
        g_value_set_string (value, priv->mime_type);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mcp_resource_template_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    McpResourceTemplate *self = MCP_RESOURCE_TEMPLATE (object);
    McpResourceTemplatePrivate *priv = mcp_resource_template_get_instance_private (self);

    switch (prop_id)
    {
    case TEMPLATE_PROP_URI_TEMPLATE:
        g_free (priv->uri_template);
        priv->uri_template = g_value_dup_string (value);
        break;
    case TEMPLATE_PROP_NAME:
        g_free (priv->name);
        priv->name = g_value_dup_string (value);
        break;
    case TEMPLATE_PROP_TITLE:
        g_free (priv->title);
        priv->title = g_value_dup_string (value);
        break;
    case TEMPLATE_PROP_DESCRIPTION:
        g_free (priv->description);
        priv->description = g_value_dup_string (value);
        break;
    case TEMPLATE_PROP_MIME_TYPE:
        g_free (priv->mime_type);
        priv->mime_type = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mcp_resource_template_class_init (McpResourceTemplateClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = mcp_resource_template_finalize;
    object_class->get_property = mcp_resource_template_get_property;
    object_class->set_property = mcp_resource_template_set_property;

    /**
     * McpResourceTemplate:uri-template:
     *
     * The URI template (RFC 6570) for matching resources.
     */
    template_properties[TEMPLATE_PROP_URI_TEMPLATE] =
        g_param_spec_string ("uri-template",
                             "URI Template",
                             "The URI template (RFC 6570) for matching resources",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    /**
     * McpResourceTemplate:name:
     *
     * The programmatic name of the template.
     */
    template_properties[TEMPLATE_PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The programmatic name of the template",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    /**
     * McpResourceTemplate:title:
     *
     * Human-readable title for the template, intended for UI display.
     */
    template_properties[TEMPLATE_PROP_TITLE] =
        g_param_spec_string ("title",
                             "Title",
                             "Human-readable title for the template",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpResourceTemplate:description:
     *
     * Human-readable description of what resources this template matches.
     */
    template_properties[TEMPLATE_PROP_DESCRIPTION] =
        g_param_spec_string ("description",
                             "Description",
                             "Human-readable description of the template",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpResourceTemplate:mime-type:
     *
     * The MIME type for all resources matching this template.
     */
    template_properties[TEMPLATE_PROP_MIME_TYPE] =
        g_param_spec_string ("mime-type",
                             "MIME Type",
                             "The MIME type for resources matching this template",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, TEMPLATE_N_PROPS, template_properties);
}

static void
mcp_resource_template_init (McpResourceTemplate *self)
{
    /* No default initialization needed */
}

/**
 * mcp_resource_template_new:
 * @uri_template: the URI template (RFC 6570)
 * @name: the template name
 *
 * Creates a new resource template definition.
 *
 * Returns: (transfer full): a new #McpResourceTemplate
 */
McpResourceTemplate *
mcp_resource_template_new (const gchar *uri_template,
                           const gchar *name)
{
    return g_object_new (MCP_TYPE_RESOURCE_TEMPLATE,
                         "uri-template", uri_template,
                         "name", name,
                         NULL);
}

/**
 * mcp_resource_template_get_uri_template:
 * @self: an #McpResourceTemplate
 *
 * Gets the URI template.
 *
 * Returns: (transfer none): the URI template
 */
const gchar *
mcp_resource_template_get_uri_template (McpResourceTemplate *self)
{
    McpResourceTemplatePrivate *priv;

    g_return_val_if_fail (MCP_IS_RESOURCE_TEMPLATE (self), NULL);

    priv = mcp_resource_template_get_instance_private (self);
    return priv->uri_template;
}

/**
 * mcp_resource_template_set_uri_template:
 * @self: an #McpResourceTemplate
 * @uri_template: the URI template
 *
 * Sets the URI template.
 */
void
mcp_resource_template_set_uri_template (McpResourceTemplate *self,
                                        const gchar         *uri_template)
{
    g_return_if_fail (MCP_IS_RESOURCE_TEMPLATE (self));
    g_object_set (self, "uri-template", uri_template, NULL);
}

/**
 * mcp_resource_template_get_name:
 * @self: an #McpResourceTemplate
 *
 * Gets the programmatic name of the template.
 *
 * Returns: (transfer none): the template name
 */
const gchar *
mcp_resource_template_get_name (McpResourceTemplate *self)
{
    McpResourceTemplatePrivate *priv;

    g_return_val_if_fail (MCP_IS_RESOURCE_TEMPLATE (self), NULL);

    priv = mcp_resource_template_get_instance_private (self);
    return priv->name;
}

/**
 * mcp_resource_template_set_name:
 * @self: an #McpResourceTemplate
 * @name: the template name
 *
 * Sets the programmatic name of the template.
 */
void
mcp_resource_template_set_name (McpResourceTemplate *self,
                                const gchar         *name)
{
    g_return_if_fail (MCP_IS_RESOURCE_TEMPLATE (self));
    g_object_set (self, "name", name, NULL);
}

/**
 * mcp_resource_template_get_title:
 * @self: an #McpResourceTemplate
 *
 * Gets the human-readable title of the template.
 *
 * Returns: (transfer none) (nullable): the template title
 */
const gchar *
mcp_resource_template_get_title (McpResourceTemplate *self)
{
    McpResourceTemplatePrivate *priv;

    g_return_val_if_fail (MCP_IS_RESOURCE_TEMPLATE (self), NULL);

    priv = mcp_resource_template_get_instance_private (self);
    return priv->title;
}

/**
 * mcp_resource_template_set_title:
 * @self: an #McpResourceTemplate
 * @title: (nullable): the template title
 *
 * Sets the human-readable title of the template.
 */
void
mcp_resource_template_set_title (McpResourceTemplate *self,
                                 const gchar         *title)
{
    g_return_if_fail (MCP_IS_RESOURCE_TEMPLATE (self));
    g_object_set (self, "title", title, NULL);
}

/**
 * mcp_resource_template_get_description:
 * @self: an #McpResourceTemplate
 *
 * Gets the description of the template.
 *
 * Returns: (transfer none) (nullable): the template description
 */
const gchar *
mcp_resource_template_get_description (McpResourceTemplate *self)
{
    McpResourceTemplatePrivate *priv;

    g_return_val_if_fail (MCP_IS_RESOURCE_TEMPLATE (self), NULL);

    priv = mcp_resource_template_get_instance_private (self);
    return priv->description;
}

/**
 * mcp_resource_template_set_description:
 * @self: an #McpResourceTemplate
 * @description: (nullable): the template description
 *
 * Sets the description of the template.
 */
void
mcp_resource_template_set_description (McpResourceTemplate *self,
                                       const gchar         *description)
{
    g_return_if_fail (MCP_IS_RESOURCE_TEMPLATE (self));
    g_object_set (self, "description", description, NULL);
}

/**
 * mcp_resource_template_get_mime_type:
 * @self: an #McpResourceTemplate
 *
 * Gets the MIME type for resources matching this template.
 *
 * Returns: (transfer none) (nullable): the MIME type
 */
const gchar *
mcp_resource_template_get_mime_type (McpResourceTemplate *self)
{
    McpResourceTemplatePrivate *priv;

    g_return_val_if_fail (MCP_IS_RESOURCE_TEMPLATE (self), NULL);

    priv = mcp_resource_template_get_instance_private (self);
    return priv->mime_type;
}

/**
 * mcp_resource_template_set_mime_type:
 * @self: an #McpResourceTemplate
 * @mime_type: (nullable): the MIME type
 *
 * Sets the MIME type for resources matching this template.
 */
void
mcp_resource_template_set_mime_type (McpResourceTemplate *self,
                                     const gchar         *mime_type)
{
    g_return_if_fail (MCP_IS_RESOURCE_TEMPLATE (self));
    g_object_set (self, "mime-type", mime_type, NULL);
}

/**
 * mcp_resource_template_to_json:
 * @self: an #McpResourceTemplate
 *
 * Serializes the template definition to a JSON object for MCP protocol.
 *
 * Returns: (transfer full): a #JsonNode containing the template definition
 */
JsonNode *
mcp_resource_template_to_json (McpResourceTemplate *self)
{
    McpResourceTemplatePrivate *priv;
    JsonBuilder *builder;
    JsonNode *result;

    g_return_val_if_fail (MCP_IS_RESOURCE_TEMPLATE (self), NULL);

    priv = mcp_resource_template_get_instance_private (self);

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    /* Required: uriTemplate */
    json_builder_set_member_name (builder, "uriTemplate");
    json_builder_add_string_value (builder, priv->uri_template ? priv->uri_template : "");

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

    /* Optional: mimeType */
    if (priv->mime_type != NULL)
    {
        json_builder_set_member_name (builder, "mimeType");
        json_builder_add_string_value (builder, priv->mime_type);
    }

    json_builder_end_object (builder);

    result = json_builder_get_root (builder);
    g_object_unref (builder);

    return result;
}

/**
 * mcp_resource_template_new_from_json:
 * @node: a #JsonNode containing a template definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new template from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpResourceTemplate, or %NULL on error
 */
McpResourceTemplate *
mcp_resource_template_new_from_json (JsonNode  *node,
                                     GError   **error)
{
    JsonObject *obj;
    McpResourceTemplate *tmpl;
    const gchar *uri_template;
    const gchar *name;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Resource template definition must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    /* Required: uriTemplate */
    if (!json_object_has_member (obj, "uriTemplate"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Resource template definition missing required 'uriTemplate' field");
        return NULL;
    }
    uri_template = json_object_get_string_member (obj, "uriTemplate");

    /* Required: name */
    if (!json_object_has_member (obj, "name"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Resource template definition missing required 'name' field");
        return NULL;
    }
    name = json_object_get_string_member (obj, "name");

    tmpl = mcp_resource_template_new (uri_template, name);

    /* Optional fields */
    if (json_object_has_member (obj, "title"))
        mcp_resource_template_set_title (tmpl, json_object_get_string_member (obj, "title"));

    if (json_object_has_member (obj, "description"))
        mcp_resource_template_set_description (tmpl, json_object_get_string_member (obj, "description"));

    if (json_object_has_member (obj, "mimeType"))
        mcp_resource_template_set_mime_type (tmpl, json_object_get_string_member (obj, "mimeType"));

    return tmpl;
}
