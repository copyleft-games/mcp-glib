/*
 * mcp-tool-provider.h - Tool provider interface for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the interface for objects that provide tools.
 * The MCP server implements this interface and delegates to registered tools.
 */

#ifndef MCP_TOOL_PROVIDER_H
#define MCP_TOOL_PROVIDER_H

#if !defined(MCP_INSIDE) && !defined(MCP_COMPILATION)
#error "Only <mcp/mcp.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <mcp/mcp-tool.h>

G_BEGIN_DECLS

#define MCP_TYPE_TOOL_PROVIDER (mcp_tool_provider_get_type ())

G_DECLARE_INTERFACE (McpToolProvider, mcp_tool_provider, MCP, TOOL_PROVIDER, GObject)

/**
 * McpToolResult:
 *
 * An opaque structure representing the result of a tool invocation.
 * Contains the content items and error status.
 */
typedef struct _McpToolResult McpToolResult;

/**
 * mcp_tool_result_new:
 * @is_error: whether the result represents an error
 *
 * Creates a new tool result.
 *
 * Returns: (transfer full): a new #McpToolResult
 */
McpToolResult *mcp_tool_result_new (gboolean is_error);

/**
 * mcp_tool_result_ref:
 * @result: a #McpToolResult
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpToolResult
 */
McpToolResult *mcp_tool_result_ref (McpToolResult *result);

/**
 * mcp_tool_result_unref:
 * @result: a #McpToolResult
 *
 * Decrements the reference count.
 */
void mcp_tool_result_unref (McpToolResult *result);

/**
 * mcp_tool_result_get_is_error:
 * @result: a #McpToolResult
 *
 * Gets whether the result represents an error.
 *
 * Returns: %TRUE if the result is an error
 */
gboolean mcp_tool_result_get_is_error (McpToolResult *result);

/**
 * mcp_tool_result_add_text:
 * @result: a #McpToolResult
 * @text: the text content
 *
 * Adds a text content item to the result.
 */
void mcp_tool_result_add_text (McpToolResult *result,
                               const gchar   *text);

/**
 * mcp_tool_result_add_image:
 * @result: a #McpToolResult
 * @data: the base64-encoded image data
 * @mime_type: the MIME type
 *
 * Adds an image content item to the result.
 */
void mcp_tool_result_add_image (McpToolResult *result,
                                const gchar   *data,
                                const gchar   *mime_type);

/**
 * mcp_tool_result_add_resource:
 * @result: a #McpToolResult
 * @uri: the resource URI
 * @text: (nullable): the text content
 * @blob: (nullable): the base64-encoded blob content
 * @mime_type: (nullable): the MIME type
 *
 * Adds an embedded resource content item to the result.
 */
void mcp_tool_result_add_resource (McpToolResult *result,
                                   const gchar   *uri,
                                   const gchar   *text,
                                   const gchar   *blob,
                                   const gchar   *mime_type);

/**
 * mcp_tool_result_get_content:
 * @result: a #McpToolResult
 *
 * Gets the content items as a JSON array.
 *
 * Returns: (transfer none): the content array
 */
JsonArray *mcp_tool_result_get_content (McpToolResult *result);

/**
 * mcp_tool_result_to_json:
 * @result: a #McpToolResult
 *
 * Serializes the result to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_tool_result_to_json (McpToolResult *result);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (McpToolResult, mcp_tool_result_unref)

/**
 * McpToolProviderInterface:
 * @parent_iface: the parent interface
 * @list_tools: lists available tools
 * @call_tool_async: calls a tool asynchronously
 * @call_tool_finish: completes an asynchronous tool call
 *
 * The interface structure for #McpToolProvider.
 */
struct _McpToolProviderInterface
{
    GTypeInterface parent_iface;

    /*< public >*/

    /**
     * McpToolProviderInterface::list_tools:
     * @self: an #McpToolProvider
     *
     * Lists the available tools.
     *
     * Returns: (transfer full) (element-type McpTool): a list of tools
     */
    GList *(*list_tools) (McpToolProvider *self);

    /**
     * McpToolProviderInterface::call_tool_async:
     * @self: an #McpToolProvider
     * @name: the tool name
     * @arguments: (nullable): the arguments as a JSON object
     * @cancellable: (nullable): a #GCancellable
     * @callback: (scope async): callback to call when complete
     * @user_data: (closure): user data for @callback
     *
     * Calls a tool asynchronously.
     */
    void (*call_tool_async) (McpToolProvider     *self,
                             const gchar         *name,
                             JsonObject          *arguments,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data);

    /**
     * McpToolProviderInterface::call_tool_finish:
     * @self: an #McpToolProvider
     * @result: the #GAsyncResult
     * @error: (nullable): return location for a #GError
     *
     * Completes an asynchronous tool call.
     *
     * Returns: (transfer full) (nullable): the #McpToolResult, or %NULL on error
     */
    McpToolResult *(*call_tool_finish) (McpToolProvider  *self,
                                        GAsyncResult     *result,
                                        GError          **error);

    /*< private >*/
    gpointer padding[8];
};

/**
 * mcp_tool_provider_list_tools:
 * @self: an #McpToolProvider
 *
 * Lists the available tools.
 *
 * Returns: (transfer full) (element-type McpTool): a list of tools
 */
GList *mcp_tool_provider_list_tools (McpToolProvider *self);

/**
 * mcp_tool_provider_call_tool_async:
 * @self: an #McpToolProvider
 * @name: the tool name
 * @arguments: (nullable): the arguments as a JSON object
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Calls a tool asynchronously.
 */
void mcp_tool_provider_call_tool_async (McpToolProvider     *self,
                                        const gchar         *name,
                                        JsonObject          *arguments,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data);

/**
 * mcp_tool_provider_call_tool_finish:
 * @self: an #McpToolProvider
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous tool call.
 *
 * Returns: (transfer full) (nullable): the #McpToolResult, or %NULL on error
 */
McpToolResult *mcp_tool_provider_call_tool_finish (McpToolProvider  *self,
                                                   GAsyncResult     *result,
                                                   GError          **error);

G_END_DECLS

#endif /* MCP_TOOL_PROVIDER_H */
