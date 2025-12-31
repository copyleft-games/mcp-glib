/*
 * mcp-resource-provider.h - Resource provider interface for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the interface for objects that provide resources.
 * The MCP server implements this interface and delegates to registered resources.
 */

#ifndef MCP_RESOURCE_PROVIDER_H
#define MCP_RESOURCE_PROVIDER_H


#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include "mcp-resource.h"

G_BEGIN_DECLS

#define MCP_TYPE_RESOURCE_PROVIDER (mcp_resource_provider_get_type ())

G_DECLARE_INTERFACE (McpResourceProvider, mcp_resource_provider, MCP, RESOURCE_PROVIDER, GObject)

/**
 * McpResourceContents:
 *
 * An opaque structure representing the contents of a resource.
 * Contains either text or blob data.
 */
typedef struct _McpResourceContents McpResourceContents;

/**
 * mcp_resource_contents_new_text:
 * @uri: the resource URI
 * @text: the text content
 * @mime_type: (nullable): the MIME type
 *
 * Creates a new resource contents with text.
 *
 * Returns: (transfer full): a new #McpResourceContents
 */
McpResourceContents *mcp_resource_contents_new_text (const gchar *uri,
                                                     const gchar *text,
                                                     const gchar *mime_type);

/**
 * mcp_resource_contents_new_blob:
 * @uri: the resource URI
 * @blob: the base64-encoded blob data
 * @mime_type: (nullable): the MIME type
 *
 * Creates a new resource contents with binary data.
 *
 * Returns: (transfer full): a new #McpResourceContents
 */
McpResourceContents *mcp_resource_contents_new_blob (const gchar *uri,
                                                     const gchar *blob,
                                                     const gchar *mime_type);

/**
 * mcp_resource_contents_ref:
 * @contents: a #McpResourceContents
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpResourceContents
 */
McpResourceContents *mcp_resource_contents_ref (McpResourceContents *contents);

/**
 * mcp_resource_contents_unref:
 * @contents: a #McpResourceContents
 *
 * Decrements the reference count.
 */
void mcp_resource_contents_unref (McpResourceContents *contents);

/**
 * mcp_resource_contents_get_uri:
 * @contents: a #McpResourceContents
 *
 * Gets the resource URI.
 *
 * Returns: (transfer none): the URI
 */
const gchar *mcp_resource_contents_get_uri (McpResourceContents *contents);

/**
 * mcp_resource_contents_get_mime_type:
 * @contents: a #McpResourceContents
 *
 * Gets the MIME type.
 *
 * Returns: (transfer none) (nullable): the MIME type, or %NULL
 */
const gchar *mcp_resource_contents_get_mime_type (McpResourceContents *contents);

/**
 * mcp_resource_contents_get_text:
 * @contents: a #McpResourceContents
 *
 * Gets the text content.
 *
 * Returns: (transfer none) (nullable): the text, or %NULL if blob
 */
const gchar *mcp_resource_contents_get_text (McpResourceContents *contents);

/**
 * mcp_resource_contents_get_blob:
 * @contents: a #McpResourceContents
 *
 * Gets the blob content (base64-encoded).
 *
 * Returns: (transfer none) (nullable): the blob, or %NULL if text
 */
const gchar *mcp_resource_contents_get_blob (McpResourceContents *contents);

/**
 * mcp_resource_contents_is_text:
 * @contents: a #McpResourceContents
 *
 * Checks if the contents is text.
 *
 * Returns: %TRUE if text, %FALSE if blob
 */
gboolean mcp_resource_contents_is_text (McpResourceContents *contents);

/**
 * mcp_resource_contents_to_json:
 * @contents: a #McpResourceContents
 *
 * Serializes the contents to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_resource_contents_to_json (McpResourceContents *contents);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (McpResourceContents, mcp_resource_contents_unref)

/**
 * McpResourceProviderInterface:
 * @parent_iface: the parent interface
 * @list_resources: lists available resources
 * @list_resource_templates: lists available resource templates
 * @read_resource_async: reads a resource asynchronously
 * @read_resource_finish: completes an asynchronous resource read
 * @subscribe: subscribes to resource updates
 * @unsubscribe: unsubscribes from resource updates
 *
 * The interface structure for #McpResourceProvider.
 */
struct _McpResourceProviderInterface
{
    GTypeInterface parent_iface;

    /*< public >*/

    /**
     * McpResourceProviderInterface::list_resources:
     * @self: an #McpResourceProvider
     *
     * Lists the available resources.
     *
     * Returns: (transfer full) (element-type McpResource): a list of resources
     */
    GList *(*list_resources) (McpResourceProvider *self);

    /**
     * McpResourceProviderInterface::list_resource_templates:
     * @self: an #McpResourceProvider
     *
     * Lists the available resource templates.
     *
     * Returns: (transfer full) (element-type McpResourceTemplate): a list of templates
     */
    GList *(*list_resource_templates) (McpResourceProvider *self);

    /**
     * McpResourceProviderInterface::read_resource_async:
     * @self: an #McpResourceProvider
     * @uri: the resource URI
     * @cancellable: (nullable): a #GCancellable
     * @callback: (scope async): callback to call when complete
     * @user_data: (closure): user data for @callback
     *
     * Reads a resource asynchronously.
     */
    void (*read_resource_async) (McpResourceProvider *self,
                                 const gchar         *uri,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data);

    /**
     * McpResourceProviderInterface::read_resource_finish:
     * @self: an #McpResourceProvider
     * @result: the #GAsyncResult
     * @error: (nullable): return location for a #GError
     *
     * Completes an asynchronous resource read.
     *
     * Returns: (transfer full) (nullable) (element-type McpResourceContents):
     *          a list of #McpResourceContents, or %NULL on error
     */
    GList *(*read_resource_finish) (McpResourceProvider  *self,
                                    GAsyncResult         *result,
                                    GError              **error);

    /**
     * McpResourceProviderInterface::subscribe:
     * @self: an #McpResourceProvider
     * @uri: the resource URI
     * @error: (nullable): return location for a #GError
     *
     * Subscribes to updates for a resource.
     *
     * Returns: %TRUE on success, %FALSE on error
     */
    gboolean (*subscribe) (McpResourceProvider  *self,
                           const gchar          *uri,
                           GError              **error);

    /**
     * McpResourceProviderInterface::unsubscribe:
     * @self: an #McpResourceProvider
     * @uri: the resource URI
     * @error: (nullable): return location for a #GError
     *
     * Unsubscribes from updates for a resource.
     *
     * Returns: %TRUE on success, %FALSE on error
     */
    gboolean (*unsubscribe) (McpResourceProvider  *self,
                             const gchar          *uri,
                             GError              **error);

    /*< private >*/
    gpointer padding[8];
};

/**
 * mcp_resource_provider_list_resources:
 * @self: an #McpResourceProvider
 *
 * Lists the available resources.
 *
 * Returns: (transfer full) (element-type McpResource): a list of resources
 */
GList *mcp_resource_provider_list_resources (McpResourceProvider *self);

/**
 * mcp_resource_provider_list_resource_templates:
 * @self: an #McpResourceProvider
 *
 * Lists the available resource templates.
 *
 * Returns: (transfer full) (element-type McpResourceTemplate): a list of templates
 */
GList *mcp_resource_provider_list_resource_templates (McpResourceProvider *self);

/**
 * mcp_resource_provider_read_resource_async:
 * @self: an #McpResourceProvider
 * @uri: the resource URI
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Reads a resource asynchronously.
 */
void mcp_resource_provider_read_resource_async (McpResourceProvider *self,
                                                const gchar         *uri,
                                                GCancellable        *cancellable,
                                                GAsyncReadyCallback  callback,
                                                gpointer             user_data);

/**
 * mcp_resource_provider_read_resource_finish:
 * @self: an #McpResourceProvider
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous resource read.
 *
 * Returns: (transfer full) (nullable) (element-type McpResourceContents):
 *          a list of #McpResourceContents, or %NULL on error
 */
GList *mcp_resource_provider_read_resource_finish (McpResourceProvider  *self,
                                                   GAsyncResult         *result,
                                                   GError              **error);

/**
 * mcp_resource_provider_subscribe:
 * @self: an #McpResourceProvider
 * @uri: the resource URI
 * @error: (nullable): return location for a #GError
 *
 * Subscribes to updates for a resource.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_resource_provider_subscribe (McpResourceProvider  *self,
                                          const gchar          *uri,
                                          GError              **error);

/**
 * mcp_resource_provider_unsubscribe:
 * @self: an #McpResourceProvider
 * @uri: the resource URI
 * @error: (nullable): return location for a #GError
 *
 * Unsubscribes from updates for a resource.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_resource_provider_unsubscribe (McpResourceProvider  *self,
                                            const gchar          *uri,
                                            GError              **error);

G_END_DECLS

#endif /* MCP_RESOURCE_PROVIDER_H */
