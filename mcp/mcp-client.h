/*
 * mcp-client.h - MCP Client class for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the MCP client class that handles:
 * - Connection and initialization with servers
 * - Tool listing and calling
 * - Resource listing, reading, and subscribing
 * - Prompt listing and getting
 * - Handling server notifications
 */

#ifndef MCP_CLIENT_H
#define MCP_CLIENT_H

#if !defined(MCP_INSIDE) && !defined(MCP_COMPILATION)
#error "Only <mcp/mcp.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include <mcp/mcp-transport.h>
#include <mcp/mcp-session.h>
#include <mcp/mcp-capabilities.h>
#include <mcp/mcp-tool.h>
#include <mcp/mcp-resource.h>
#include <mcp/mcp-prompt.h>
#include <mcp/mcp-task.h>
#include <mcp/mcp-tool-provider.h>
#include <mcp/mcp-resource-provider.h>
#include <mcp/mcp-prompt-provider.h>

G_BEGIN_DECLS

#define MCP_TYPE_CLIENT (mcp_client_get_type ())

G_DECLARE_FINAL_TYPE (McpClient, mcp_client, MCP, CLIENT, McpSession)

/**
 * mcp_client_new:
 * @name: the client name
 * @version: the client version
 *
 * Creates a new MCP client.
 *
 * Returns: (transfer full): a new #McpClient
 */
McpClient *mcp_client_new (const gchar *name,
                           const gchar *version);

/**
 * mcp_client_get_capabilities:
 * @self: an #McpClient
 *
 * Gets the client capabilities.
 *
 * Returns: (transfer none): the #McpClientCapabilities
 */
McpClientCapabilities *mcp_client_get_capabilities (McpClient *self);

/**
 * mcp_client_get_server_capabilities:
 * @self: an #McpClient
 *
 * Gets the connected server's capabilities.
 * Only valid after initialization is complete.
 *
 * Returns: (transfer none) (nullable): the #McpServerCapabilities, or %NULL
 */
McpServerCapabilities *mcp_client_get_server_capabilities (McpClient *self);

/**
 * mcp_client_get_server_instructions:
 * @self: an #McpClient
 *
 * Gets the server's instructions for the LLM.
 * Only valid after initialization is complete.
 *
 * Returns: (transfer none) (nullable): the instructions, or %NULL
 */
const gchar *mcp_client_get_server_instructions (McpClient *self);

/* Transport management */

/**
 * mcp_client_set_transport:
 * @self: an #McpClient
 * @transport: (transfer none): the #McpTransport
 *
 * Sets the transport to use for communication.
 */
void mcp_client_set_transport (McpClient    *self,
                               McpTransport *transport);

/**
 * mcp_client_get_transport:
 * @self: an #McpClient
 *
 * Gets the transport.
 *
 * Returns: (transfer none) (nullable): the #McpTransport
 */
McpTransport *mcp_client_get_transport (McpClient *self);

/* Connection */

/**
 * mcp_client_connect_async:
 * @self: an #McpClient
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Connects to the server and performs initialization.
 */
void mcp_client_connect_async (McpClient           *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data);

/**
 * mcp_client_connect_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous connection.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_client_connect_finish (McpClient     *self,
                                    GAsyncResult  *result,
                                    GError       **error);

/**
 * mcp_client_disconnect_async:
 * @self: an #McpClient
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Disconnects from the server.
 */
void mcp_client_disconnect_async (McpClient           *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data);

/**
 * mcp_client_disconnect_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous disconnection.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_client_disconnect_finish (McpClient     *self,
                                       GAsyncResult  *result,
                                       GError       **error);

/* Tool operations */

/**
 * mcp_client_list_tools_async:
 * @self: an #McpClient
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Lists available tools from the server.
 */
void mcp_client_list_tools_async (McpClient           *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data);

/**
 * mcp_client_list_tools_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous tool listing.
 *
 * Returns: (transfer full) (element-type McpTool): a list of tools, or %NULL on error
 */
GList *mcp_client_list_tools_finish (McpClient     *self,
                                     GAsyncResult  *result,
                                     GError       **error);

/**
 * mcp_client_call_tool_async:
 * @self: an #McpClient
 * @name: the tool name
 * @arguments: (nullable): the arguments as a #JsonObject
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Calls a tool on the server.
 */
void mcp_client_call_tool_async (McpClient           *self,
                                 const gchar         *name,
                                 JsonObject          *arguments,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data);

/**
 * mcp_client_call_tool_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous tool call.
 *
 * Returns: (transfer full) (nullable): the #McpToolResult, or %NULL on error
 */
McpToolResult *mcp_client_call_tool_finish (McpClient     *self,
                                            GAsyncResult  *result,
                                            GError       **error);

/* Resource operations */

/**
 * mcp_client_list_resources_async:
 * @self: an #McpClient
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Lists available resources from the server.
 */
void mcp_client_list_resources_async (McpClient           *self,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data);

/**
 * mcp_client_list_resources_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous resource listing.
 *
 * Returns: (transfer full) (element-type McpResource): a list of resources, or %NULL on error
 */
GList *mcp_client_list_resources_finish (McpClient     *self,
                                         GAsyncResult  *result,
                                         GError       **error);

/**
 * mcp_client_list_resource_templates_async:
 * @self: an #McpClient
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Lists available resource templates from the server.
 */
void mcp_client_list_resource_templates_async (McpClient           *self,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data);

/**
 * mcp_client_list_resource_templates_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous resource template listing.
 *
 * Returns: (transfer full) (element-type McpResourceTemplate): a list of templates, or %NULL on error
 */
GList *mcp_client_list_resource_templates_finish (McpClient     *self,
                                                  GAsyncResult  *result,
                                                  GError       **error);

/**
 * mcp_client_read_resource_async:
 * @self: an #McpClient
 * @uri: the resource URI
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Reads a resource from the server.
 */
void mcp_client_read_resource_async (McpClient           *self,
                                     const gchar         *uri,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data);

/**
 * mcp_client_read_resource_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous resource read.
 *
 * Returns: (transfer full) (element-type McpResourceContents): a list of contents, or %NULL on error
 */
GList *mcp_client_read_resource_finish (McpClient     *self,
                                        GAsyncResult  *result,
                                        GError       **error);

/**
 * mcp_client_subscribe_resource_async:
 * @self: an #McpClient
 * @uri: the resource URI
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Subscribes to updates for a resource.
 */
void mcp_client_subscribe_resource_async (McpClient           *self,
                                          const gchar         *uri,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data);

/**
 * mcp_client_subscribe_resource_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous resource subscription.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_client_subscribe_resource_finish (McpClient     *self,
                                               GAsyncResult  *result,
                                               GError       **error);

/**
 * mcp_client_unsubscribe_resource_async:
 * @self: an #McpClient
 * @uri: the resource URI
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Unsubscribes from updates for a resource.
 */
void mcp_client_unsubscribe_resource_async (McpClient           *self,
                                            const gchar         *uri,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data);

/**
 * mcp_client_unsubscribe_resource_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous resource unsubscription.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_client_unsubscribe_resource_finish (McpClient     *self,
                                                 GAsyncResult  *result,
                                                 GError       **error);

/* Prompt operations */

/**
 * mcp_client_list_prompts_async:
 * @self: an #McpClient
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Lists available prompts from the server.
 */
void mcp_client_list_prompts_async (McpClient           *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data);

/**
 * mcp_client_list_prompts_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous prompt listing.
 *
 * Returns: (transfer full) (element-type McpPrompt): a list of prompts, or %NULL on error
 */
GList *mcp_client_list_prompts_finish (McpClient     *self,
                                       GAsyncResult  *result,
                                       GError       **error);

/**
 * mcp_client_get_prompt_async:
 * @self: an #McpClient
 * @name: the prompt name
 * @arguments: (nullable): the arguments as a hash table of string to string
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Gets a prompt from the server.
 */
void mcp_client_get_prompt_async (McpClient           *self,
                                  const gchar         *name,
                                  GHashTable          *arguments,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data);

/**
 * mcp_client_get_prompt_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous prompt get.
 *
 * Returns: (transfer full) (nullable): the #McpPromptResult, or %NULL on error
 */
McpPromptResult *mcp_client_get_prompt_finish (McpClient     *self,
                                               GAsyncResult  *result,
                                               GError       **error);

/* Utility */

/**
 * mcp_client_ping_async:
 * @self: an #McpClient
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Sends a ping to the server.
 */
void mcp_client_ping_async (McpClient           *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data);

/**
 * mcp_client_ping_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous ping.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_client_ping_finish (McpClient     *self,
                                 GAsyncResult  *result,
                                 GError       **error);

/* Tasks API (Experimental) */

/**
 * mcp_client_get_task_async:
 * @self: an #McpClient
 * @task_id: the task identifier
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Gets the status of a task from the server.
 */
void mcp_client_get_task_async (McpClient           *self,
                                const gchar         *task_id,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data);

/**
 * mcp_client_get_task_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous task get.
 *
 * Returns: (transfer full) (nullable): the #McpTask, or %NULL on error
 */
McpTask *mcp_client_get_task_finish (McpClient     *self,
                                     GAsyncResult  *result,
                                     GError       **error);

/**
 * mcp_client_get_task_result_async:
 * @self: an #McpClient
 * @task_id: the task identifier
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Gets the result of a completed task from the server.
 */
void mcp_client_get_task_result_async (McpClient           *self,
                                       const gchar         *task_id,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data);

/**
 * mcp_client_get_task_result_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous task result get.
 *
 * Returns: (transfer full) (nullable): the #McpToolResult, or %NULL on error
 */
McpToolResult *mcp_client_get_task_result_finish (McpClient     *self,
                                                  GAsyncResult  *result,
                                                  GError       **error);

/**
 * mcp_client_cancel_task_async:
 * @self: an #McpClient
 * @task_id: the task identifier
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Cancels a running task on the server.
 */
void mcp_client_cancel_task_async (McpClient           *self,
                                   const gchar         *task_id,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data);

/**
 * mcp_client_cancel_task_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous task cancellation.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_client_cancel_task_finish (McpClient     *self,
                                        GAsyncResult  *result,
                                        GError       **error);

/**
 * mcp_client_list_tasks_async:
 * @self: an #McpClient
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Lists active tasks on the server.
 */
void mcp_client_list_tasks_async (McpClient           *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data);

/**
 * mcp_client_list_tasks_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous task listing.
 *
 * Returns: (transfer full) (element-type McpTask): a list of tasks, or %NULL on error
 */
GList *mcp_client_list_tasks_finish (McpClient     *self,
                                     GAsyncResult  *result,
                                     GError       **error);

G_END_DECLS

#endif /* MCP_CLIENT_H */
