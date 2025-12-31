/*
 * mcp-server.h - MCP Server class for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the main MCP server class that handles:
 * - Protocol initialization and handshake
 * - Tool, resource, and prompt registration
 * - Request handling and response generation
 * - Notification emission
 */

#ifndef MCP_SERVER_H
#define MCP_SERVER_H

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
#include <mcp/mcp-sampling.h>
#include <mcp/mcp-root.h>
#include <mcp/mcp-completion.h>

G_BEGIN_DECLS

#define MCP_TYPE_SERVER (mcp_server_get_type ())

G_DECLARE_FINAL_TYPE (McpServer, mcp_server, MCP, SERVER, McpSession)

/**
 * McpToolHandler:
 * @server: the #McpServer
 * @name: the tool name
 * @arguments: (nullable): the arguments
 * @user_data: user data
 *
 * Callback function for handling tool calls.
 *
 * Returns: (transfer full): the #McpToolResult
 */
typedef McpToolResult *(*McpToolHandler) (McpServer   *server,
                                          const gchar *name,
                                          JsonObject  *arguments,
                                          gpointer     user_data);

/**
 * McpResourceHandler:
 * @server: the #McpServer
 * @uri: the resource URI
 * @user_data: user data
 *
 * Callback function for handling resource reads.
 *
 * Returns: (transfer full) (element-type McpResourceContents): the contents list
 */
typedef GList *(*McpResourceHandler) (McpServer   *server,
                                      const gchar *uri,
                                      gpointer     user_data);

/**
 * McpPromptHandler:
 * @server: the #McpServer
 * @name: the prompt name
 * @arguments: (nullable): the arguments
 * @user_data: user data
 *
 * Callback function for handling prompt gets.
 *
 * Returns: (transfer full): the #McpPromptResult
 */
typedef McpPromptResult *(*McpPromptHandler) (McpServer  *server,
                                              const gchar *name,
                                              GHashTable  *arguments,
                                              gpointer     user_data);

/**
 * McpAsyncToolHandler:
 * @server: the #McpServer
 * @task: (transfer none): the #McpTask for this operation
 * @name: the tool name
 * @arguments: (nullable): the arguments
 * @user_data: user data
 *
 * Callback function for handling asynchronous tool calls.
 * The handler should update the task status as it progresses.
 * When complete, call mcp_server_complete_task() with the result.
 *
 * If the operation can complete synchronously, return the result
 * and the task will be marked completed automatically.
 *
 * Returns: (transfer full) (nullable): the #McpToolResult if completed synchronously, or %NULL
 */
typedef McpToolResult *(*McpAsyncToolHandler) (McpServer   *server,
                                               McpTask     *task,
                                               const gchar *name,
                                               JsonObject  *arguments,
                                               gpointer     user_data);

/**
 * McpCompletionHandler:
 * @server: the #McpServer
 * @ref_type: the reference type ("ref/prompt" or "ref/resource")
 * @ref_name: the reference name (prompt name or resource URI)
 * @argument_name: (nullable): the argument name for prompts
 * @argument_value: the current argument value to complete
 * @user_data: user data
 *
 * Callback function for handling completion requests.
 * Returns autocomplete suggestions for the given argument value.
 *
 * Returns: (transfer full): the #McpCompletionResult
 */
typedef McpCompletionResult *(*McpCompletionHandler) (McpServer   *server,
                                                      const gchar *ref_type,
                                                      const gchar *ref_name,
                                                      const gchar *argument_name,
                                                      const gchar *argument_value,
                                                      gpointer     user_data);

/**
 * mcp_server_new:
 * @name: the server name
 * @version: the server version
 *
 * Creates a new MCP server.
 *
 * Returns: (transfer full): a new #McpServer
 */
McpServer *mcp_server_new (const gchar *name,
                           const gchar *version);

/**
 * mcp_server_get_capabilities:
 * @self: an #McpServer
 *
 * Gets the server capabilities.
 *
 * Returns: (transfer none): the #McpServerCapabilities
 */
McpServerCapabilities *mcp_server_get_capabilities (McpServer *self);

/**
 * mcp_server_get_client_capabilities:
 * @self: an #McpServer
 *
 * Gets the connected client's capabilities.
 * Only valid after initialization is complete.
 *
 * Returns: (transfer none) (nullable): the #McpClientCapabilities, or %NULL
 */
McpClientCapabilities *mcp_server_get_client_capabilities (McpServer *self);

/**
 * mcp_server_set_instructions:
 * @self: an #McpServer
 * @instructions: (nullable): the instructions text
 *
 * Sets the server instructions that help the LLM use the server.
 */
void mcp_server_set_instructions (McpServer   *self,
                                  const gchar *instructions);

/**
 * mcp_server_get_instructions:
 * @self: an #McpServer
 *
 * Gets the server instructions.
 *
 * Returns: (transfer none) (nullable): the instructions
 */
const gchar *mcp_server_get_instructions (McpServer *self);

/* Transport management */

/**
 * mcp_server_set_transport:
 * @self: an #McpServer
 * @transport: (transfer none): the #McpTransport
 *
 * Sets the transport to use for communication.
 */
void mcp_server_set_transport (McpServer    *self,
                               McpTransport *transport);

/**
 * mcp_server_get_transport:
 * @self: an #McpServer
 *
 * Gets the transport.
 *
 * Returns: (transfer none) (nullable): the #McpTransport
 */
McpTransport *mcp_server_get_transport (McpServer *self);

/**
 * mcp_server_start_async:
 * @self: an #McpServer
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Starts the server and waits for initialization.
 */
void mcp_server_start_async (McpServer           *self,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data);

/**
 * mcp_server_start_finish:
 * @self: an #McpServer
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous server start.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_server_start_finish (McpServer     *self,
                                  GAsyncResult  *result,
                                  GError       **error);

/**
 * mcp_server_run:
 * @self: an #McpServer
 * @error: (nullable): return location for a #GError
 *
 * Runs the server synchronously until disconnected or error.
 * This is a convenience method that blocks the main loop.
 *
 * Returns: %TRUE on clean shutdown, %FALSE on error
 */
gboolean mcp_server_run (McpServer  *self,
                         GError    **error);

/**
 * mcp_server_stop:
 * @self: an #McpServer
 *
 * Stops the server.
 */
void mcp_server_stop (McpServer *self);

/* Tool management */

/**
 * mcp_server_add_tool:
 * @self: an #McpServer
 * @tool: (transfer none): the #McpTool to add
 * @handler: (scope notified) (nullable): the handler function
 * @user_data: (closure): user data for @handler
 * @destroy: (destroy user_data): destroy notify for @user_data
 *
 * Adds a tool to the server.
 */
void mcp_server_add_tool (McpServer      *self,
                          McpTool        *tool,
                          McpToolHandler  handler,
                          gpointer        user_data,
                          GDestroyNotify  destroy);

/**
 * mcp_server_remove_tool:
 * @self: an #McpServer
 * @name: the tool name
 *
 * Removes a tool from the server.
 *
 * Returns: %TRUE if the tool was removed
 */
gboolean mcp_server_remove_tool (McpServer   *self,
                                 const gchar *name);

/**
 * mcp_server_list_tools:
 * @self: an #McpServer
 *
 * Lists all registered tools.
 *
 * Returns: (transfer full) (element-type McpTool): a list of tools
 */
GList *mcp_server_list_tools (McpServer *self);

/* Resource management */

/**
 * mcp_server_add_resource:
 * @self: an #McpServer
 * @resource: (transfer none): the #McpResource to add
 * @handler: (scope notified) (nullable): the handler function
 * @user_data: (closure): user data for @handler
 * @destroy: (destroy user_data): destroy notify for @user_data
 *
 * Adds a resource to the server.
 */
void mcp_server_add_resource (McpServer          *self,
                              McpResource        *resource,
                              McpResourceHandler  handler,
                              gpointer            user_data,
                              GDestroyNotify      destroy);

/**
 * mcp_server_add_resource_template:
 * @self: an #McpServer
 * @templ: (transfer none): the #McpResourceTemplate to add
 * @handler: (scope notified) (nullable): the handler function
 * @user_data: (closure): user data for @handler
 * @destroy: (destroy user_data): destroy notify for @user_data
 *
 * Adds a resource template to the server.
 */
void mcp_server_add_resource_template (McpServer            *self,
                                       McpResourceTemplate  *templ,
                                       McpResourceHandler    handler,
                                       gpointer              user_data,
                                       GDestroyNotify        destroy);

/**
 * mcp_server_remove_resource:
 * @self: an #McpServer
 * @uri: the resource URI
 *
 * Removes a resource from the server.
 *
 * Returns: %TRUE if the resource was removed
 */
gboolean mcp_server_remove_resource (McpServer   *self,
                                     const gchar *uri);

/**
 * mcp_server_list_resources:
 * @self: an #McpServer
 *
 * Lists all registered resources.
 *
 * Returns: (transfer full) (element-type McpResource): a list of resources
 */
GList *mcp_server_list_resources (McpServer *self);

/**
 * mcp_server_list_resource_templates:
 * @self: an #McpServer
 *
 * Lists all registered resource templates.
 *
 * Returns: (transfer full) (element-type McpResourceTemplate): a list of templates
 */
GList *mcp_server_list_resource_templates (McpServer *self);

/**
 * mcp_server_notify_resource_updated:
 * @self: an #McpServer
 * @uri: the resource URI
 *
 * Notifies the client that a resource has been updated.
 */
void mcp_server_notify_resource_updated (McpServer   *self,
                                         const gchar *uri);

/* Prompt management */

/**
 * mcp_server_add_prompt:
 * @self: an #McpServer
 * @prompt: (transfer none): the #McpPrompt to add
 * @handler: (scope notified) (nullable): the handler function
 * @user_data: (closure): user data for @handler
 * @destroy: (destroy user_data): destroy notify for @user_data
 *
 * Adds a prompt to the server.
 */
void mcp_server_add_prompt (McpServer        *self,
                            McpPrompt        *prompt,
                            McpPromptHandler  handler,
                            gpointer          user_data,
                            GDestroyNotify    destroy);

/**
 * mcp_server_remove_prompt:
 * @self: an #McpServer
 * @name: the prompt name
 *
 * Removes a prompt from the server.
 *
 * Returns: %TRUE if the prompt was removed
 */
gboolean mcp_server_remove_prompt (McpServer   *self,
                                   const gchar *name);

/**
 * mcp_server_list_prompts:
 * @self: an #McpServer
 *
 * Lists all registered prompts.
 *
 * Returns: (transfer full) (element-type McpPrompt): a list of prompts
 */
GList *mcp_server_list_prompts (McpServer *self);

/* Notifications */

/**
 * mcp_server_notify_tools_changed:
 * @self: an #McpServer
 *
 * Notifies the client that the tool list has changed.
 */
void mcp_server_notify_tools_changed (McpServer *self);

/**
 * mcp_server_notify_resources_changed:
 * @self: an #McpServer
 *
 * Notifies the client that the resource list has changed.
 */
void mcp_server_notify_resources_changed (McpServer *self);

/**
 * mcp_server_notify_prompts_changed:
 * @self: an #McpServer
 *
 * Notifies the client that the prompt list has changed.
 */
void mcp_server_notify_prompts_changed (McpServer *self);

/**
 * mcp_server_emit_log:
 * @self: an #McpServer
 * @level: the log level
 * @logger: (nullable): the logger name
 * @data: the log data (can be any JSON value)
 *
 * Emits a log notification to the client.
 */
void mcp_server_emit_log (McpServer   *self,
                          McpLogLevel  level,
                          const gchar *logger,
                          JsonNode    *data);

/**
 * mcp_server_send_progress:
 * @self: an #McpServer
 * @token: the progress token
 * @progress: the current progress (0.0 to 1.0)
 * @total: (nullable): the total amount, or -1 if unknown
 *
 * Sends a progress notification for a long-running operation.
 */
void mcp_server_send_progress (McpServer   *self,
                               const gchar *token,
                               gdouble      progress,
                               gint64       total);

/* Tasks API (Experimental) */

/**
 * mcp_server_add_async_tool:
 * @self: an #McpServer
 * @tool: (transfer none): the #McpTool to add
 * @handler: (scope notified) (nullable): the async handler function
 * @user_data: (closure): user data for @handler
 * @destroy: (destroy user_data): destroy notify for @user_data
 *
 * Adds a tool with asynchronous execution support (Tasks API).
 * The tool will be handled using tasks for long-running operations.
 */
void mcp_server_add_async_tool (McpServer          *self,
                                McpTool            *tool,
                                McpAsyncToolHandler handler,
                                gpointer            user_data,
                                GDestroyNotify      destroy);

/**
 * mcp_server_get_task:
 * @self: an #McpServer
 * @task_id: the task identifier
 *
 * Gets a task by its identifier.
 *
 * Returns: (transfer none) (nullable): the #McpTask, or %NULL if not found
 */
McpTask *mcp_server_get_task (McpServer   *self,
                              const gchar *task_id);

/**
 * mcp_server_list_tasks:
 * @self: an #McpServer
 *
 * Lists all active tasks.
 *
 * Returns: (transfer full) (element-type McpTask): a list of tasks
 */
GList *mcp_server_list_tasks (McpServer *self);

/**
 * mcp_server_complete_task:
 * @self: an #McpServer
 * @task_id: the task identifier
 * @result: (transfer full) (nullable): the tool result
 * @error_message: (nullable): an error message if the task failed
 *
 * Marks a task as completed with the given result.
 * This sends a task status notification to the client.
 *
 * Returns: %TRUE if the task was found and completed
 */
gboolean mcp_server_complete_task (McpServer     *self,
                                   const gchar   *task_id,
                                   McpToolResult *result,
                                   const gchar   *error_message);

/**
 * mcp_server_fail_task:
 * @self: an #McpServer
 * @task_id: the task identifier
 * @error_message: the error message
 *
 * Marks a task as failed with the given error message.
 * This sends a task status notification to the client.
 *
 * Returns: %TRUE if the task was found and failed
 */
gboolean mcp_server_fail_task (McpServer   *self,
                               const gchar *task_id,
                               const gchar *error_message);

/**
 * mcp_server_update_task_status:
 * @self: an #McpServer
 * @task_id: the task identifier
 * @status: the new status
 * @message: (nullable): a status message
 *
 * Updates the status of a task and sends a notification.
 *
 * Returns: %TRUE if the task was found and updated
 */
gboolean mcp_server_update_task_status (McpServer     *self,
                                        const gchar   *task_id,
                                        McpTaskStatus  status,
                                        const gchar   *message);

/**
 * mcp_server_cancel_task:
 * @self: an #McpServer
 * @task_id: the task identifier
 *
 * Cancels a running task.
 *
 * Returns: %TRUE if the task was found and cancelled
 */
gboolean mcp_server_cancel_task (McpServer   *self,
                                 const gchar *task_id);

/**
 * mcp_server_notify_task_status:
 * @self: an #McpServer
 * @task: the #McpTask that was updated
 *
 * Sends a task status notification to the client.
 */
void mcp_server_notify_task_status (McpServer *self,
                                    McpTask   *task);

/* Sampling API (Server requests LLM sampling from client) */

/**
 * mcp_server_request_sampling_async:
 * @self: an #McpServer
 * @messages: (element-type McpSamplingMessage): list of messages for the LLM
 * @model_preferences: (nullable): model preferences
 * @system_prompt: (nullable): system prompt for the LLM
 * @max_tokens: maximum tokens to generate
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Requests the client to perform LLM sampling.
 * This is a server-to-client request (role reversal).
 */
void mcp_server_request_sampling_async (McpServer           *self,
                                        GList               *messages,
                                        McpModelPreferences *model_preferences,
                                        const gchar         *system_prompt,
                                        gint64               max_tokens,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data);

/**
 * mcp_server_request_sampling_finish:
 * @self: an #McpServer
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous sampling request.
 *
 * Returns: (transfer full): the #McpSamplingResult, or %NULL on error
 */
McpSamplingResult *mcp_server_request_sampling_finish (McpServer     *self,
                                                       GAsyncResult  *result,
                                                       GError       **error);

/* Roots API (Server requests root list from client) */

/**
 * mcp_server_list_roots_async:
 * @self: an #McpServer
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Requests the list of roots from the client.
 * This is a server-to-client request (role reversal).
 */
void mcp_server_list_roots_async (McpServer           *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data);

/**
 * mcp_server_list_roots_finish:
 * @self: an #McpServer
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous roots list request.
 *
 * Returns: (transfer full) (element-type McpRoot): list of roots, or %NULL on error
 */
GList *mcp_server_list_roots_finish (McpServer     *self,
                                     GAsyncResult  *result,
                                     GError       **error);

/* Completion API */

/**
 * mcp_server_set_completion_handler:
 * @self: an #McpServer
 * @handler: (scope notified) (nullable): the handler function
 * @user_data: (closure): user data for @handler
 * @destroy: (destroy user_data): destroy notify for @user_data
 *
 * Sets the handler for completion requests.
 * The completion handler provides autocomplete suggestions for
 * prompt arguments and resource URIs.
 */
void mcp_server_set_completion_handler (McpServer            *self,
                                        McpCompletionHandler  handler,
                                        gpointer              user_data,
                                        GDestroyNotify        destroy);

G_END_DECLS

#endif /* MCP_SERVER_H */
