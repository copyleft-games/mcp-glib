/*
 * mcp-resource.h - Resource definition for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef MCP_RESOURCE_H
#define MCP_RESOURCE_H

#if !defined(MCP_INSIDE) && !defined(MCP_COMPILATION)
#error "Only <mcp/mcp.h> can be included directly."
#endif

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define MCP_TYPE_RESOURCE (mcp_resource_get_type ())

G_DECLARE_DERIVABLE_TYPE (McpResource, mcp_resource, MCP, RESOURCE, GObject)

/**
 * McpResourceClass:
 * @parent_class: the parent class
 *
 * The class structure for #McpResource.
 */
struct _McpResourceClass
{
    GObjectClass parent_class;

    /*< private >*/
    gpointer padding[8];
};

/**
 * mcp_resource_new:
 * @uri: the resource URI
 * @name: the resource name
 *
 * Creates a new resource definition.
 *
 * Returns: (transfer full): a new #McpResource
 */
McpResource *mcp_resource_new (const gchar *uri,
                               const gchar *name);

/**
 * mcp_resource_get_uri:
 * @self: an #McpResource
 *
 * Gets the URI of the resource.
 *
 * Returns: (transfer none): the resource URI
 */
const gchar *mcp_resource_get_uri (McpResource *self);

/**
 * mcp_resource_set_uri:
 * @self: an #McpResource
 * @uri: the resource URI
 *
 * Sets the URI of the resource.
 */
void mcp_resource_set_uri (McpResource *self,
                           const gchar *uri);

/**
 * mcp_resource_get_name:
 * @self: an #McpResource
 *
 * Gets the programmatic name of the resource.
 *
 * Returns: (transfer none): the resource name
 */
const gchar *mcp_resource_get_name (McpResource *self);

/**
 * mcp_resource_set_name:
 * @self: an #McpResource
 * @name: the resource name
 *
 * Sets the programmatic name of the resource.
 */
void mcp_resource_set_name (McpResource *self,
                            const gchar *name);

/**
 * mcp_resource_get_title:
 * @self: an #McpResource
 *
 * Gets the human-readable title of the resource.
 *
 * Returns: (transfer none) (nullable): the resource title
 */
const gchar *mcp_resource_get_title (McpResource *self);

/**
 * mcp_resource_set_title:
 * @self: an #McpResource
 * @title: (nullable): the resource title
 *
 * Sets the human-readable title of the resource.
 */
void mcp_resource_set_title (McpResource *self,
                             const gchar *title);

/**
 * mcp_resource_get_description:
 * @self: an #McpResource
 *
 * Gets the description of the resource.
 *
 * Returns: (transfer none) (nullable): the resource description
 */
const gchar *mcp_resource_get_description (McpResource *self);

/**
 * mcp_resource_set_description:
 * @self: an #McpResource
 * @description: (nullable): the resource description
 *
 * Sets the description of the resource.
 */
void mcp_resource_set_description (McpResource *self,
                                   const gchar *description);

/**
 * mcp_resource_get_mime_type:
 * @self: an #McpResource
 *
 * Gets the MIME type of the resource.
 *
 * Returns: (transfer none) (nullable): the MIME type
 */
const gchar *mcp_resource_get_mime_type (McpResource *self);

/**
 * mcp_resource_set_mime_type:
 * @self: an #McpResource
 * @mime_type: (nullable): the MIME type
 *
 * Sets the MIME type of the resource.
 */
void mcp_resource_set_mime_type (McpResource *self,
                                 const gchar *mime_type);

/**
 * mcp_resource_get_size:
 * @self: an #McpResource
 *
 * Gets the size of the resource content in bytes.
 * Returns -1 if the size is unknown.
 *
 * Returns: the size in bytes, or -1 if unknown
 */
gint64 mcp_resource_get_size (McpResource *self);

/**
 * mcp_resource_set_size:
 * @self: an #McpResource
 * @size: the size in bytes, or -1 if unknown
 *
 * Sets the size of the resource content in bytes.
 */
void mcp_resource_set_size (McpResource *self,
                            gint64       size);

/**
 * mcp_resource_to_json:
 * @self: an #McpResource
 *
 * Serializes the resource definition to a JSON object for MCP protocol.
 *
 * Returns: (transfer full): a #JsonNode containing the resource definition
 */
JsonNode *mcp_resource_to_json (McpResource *self);

/**
 * mcp_resource_new_from_json:
 * @node: a #JsonNode containing a resource definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new resource from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpResource, or %NULL on error
 */
McpResource *mcp_resource_new_from_json (JsonNode  *node,
                                         GError   **error);

/* ========================================================================== */
/* McpResourceTemplate                                                        */
/* ========================================================================== */

#define MCP_TYPE_RESOURCE_TEMPLATE (mcp_resource_template_get_type ())

G_DECLARE_DERIVABLE_TYPE (McpResourceTemplate, mcp_resource_template, MCP, RESOURCE_TEMPLATE, GObject)

/**
 * McpResourceTemplateClass:
 * @parent_class: the parent class
 *
 * The class structure for #McpResourceTemplate.
 */
struct _McpResourceTemplateClass
{
    GObjectClass parent_class;

    /*< private >*/
    gpointer padding[8];
};

/**
 * mcp_resource_template_new:
 * @uri_template: the URI template (RFC 6570)
 * @name: the template name
 *
 * Creates a new resource template definition.
 *
 * Returns: (transfer full): a new #McpResourceTemplate
 */
McpResourceTemplate *mcp_resource_template_new (const gchar *uri_template,
                                                const gchar *name);

/**
 * mcp_resource_template_get_uri_template:
 * @self: an #McpResourceTemplate
 *
 * Gets the URI template.
 *
 * Returns: (transfer none): the URI template
 */
const gchar *mcp_resource_template_get_uri_template (McpResourceTemplate *self);

/**
 * mcp_resource_template_set_uri_template:
 * @self: an #McpResourceTemplate
 * @uri_template: the URI template
 *
 * Sets the URI template.
 */
void mcp_resource_template_set_uri_template (McpResourceTemplate *self,
                                             const gchar         *uri_template);

/**
 * mcp_resource_template_get_name:
 * @self: an #McpResourceTemplate
 *
 * Gets the programmatic name of the template.
 *
 * Returns: (transfer none): the template name
 */
const gchar *mcp_resource_template_get_name (McpResourceTemplate *self);

/**
 * mcp_resource_template_set_name:
 * @self: an #McpResourceTemplate
 * @name: the template name
 *
 * Sets the programmatic name of the template.
 */
void mcp_resource_template_set_name (McpResourceTemplate *self,
                                     const gchar         *name);

/**
 * mcp_resource_template_get_title:
 * @self: an #McpResourceTemplate
 *
 * Gets the human-readable title of the template.
 *
 * Returns: (transfer none) (nullable): the template title
 */
const gchar *mcp_resource_template_get_title (McpResourceTemplate *self);

/**
 * mcp_resource_template_set_title:
 * @self: an #McpResourceTemplate
 * @title: (nullable): the template title
 *
 * Sets the human-readable title of the template.
 */
void mcp_resource_template_set_title (McpResourceTemplate *self,
                                      const gchar         *title);

/**
 * mcp_resource_template_get_description:
 * @self: an #McpResourceTemplate
 *
 * Gets the description of the template.
 *
 * Returns: (transfer none) (nullable): the template description
 */
const gchar *mcp_resource_template_get_description (McpResourceTemplate *self);

/**
 * mcp_resource_template_set_description:
 * @self: an #McpResourceTemplate
 * @description: (nullable): the template description
 *
 * Sets the description of the template.
 */
void mcp_resource_template_set_description (McpResourceTemplate *self,
                                            const gchar         *description);

/**
 * mcp_resource_template_get_mime_type:
 * @self: an #McpResourceTemplate
 *
 * Gets the MIME type for resources matching this template.
 *
 * Returns: (transfer none) (nullable): the MIME type
 */
const gchar *mcp_resource_template_get_mime_type (McpResourceTemplate *self);

/**
 * mcp_resource_template_set_mime_type:
 * @self: an #McpResourceTemplate
 * @mime_type: (nullable): the MIME type
 *
 * Sets the MIME type for resources matching this template.
 */
void mcp_resource_template_set_mime_type (McpResourceTemplate *self,
                                          const gchar         *mime_type);

/**
 * mcp_resource_template_to_json:
 * @self: an #McpResourceTemplate
 *
 * Serializes the template definition to a JSON object for MCP protocol.
 *
 * Returns: (transfer full): a #JsonNode containing the template definition
 */
JsonNode *mcp_resource_template_to_json (McpResourceTemplate *self);

/**
 * mcp_resource_template_new_from_json:
 * @node: a #JsonNode containing a template definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new template from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpResourceTemplate, or %NULL on error
 */
McpResourceTemplate *mcp_resource_template_new_from_json (JsonNode  *node,
                                                          GError   **error);

G_END_DECLS

#endif /* MCP_RESOURCE_H */
