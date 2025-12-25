/*
 * mcp-prompt-provider.h - Prompt provider interface for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the interface for objects that provide prompts.
 * The MCP server implements this interface and delegates to registered prompts.
 */

#ifndef MCP_PROMPT_PROVIDER_H
#define MCP_PROMPT_PROVIDER_H

#if !defined(MCP_INSIDE) && !defined(MCP_COMPILATION)
#error "Only <mcp/mcp.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <mcp/mcp-enums.h>
#include <mcp/mcp-prompt.h>

G_BEGIN_DECLS

#define MCP_TYPE_PROMPT_PROVIDER (mcp_prompt_provider_get_type ())

G_DECLARE_INTERFACE (McpPromptProvider, mcp_prompt_provider, MCP, PROMPT_PROVIDER, GObject)

/**
 * McpPromptMessage:
 *
 * An opaque structure representing a message in a prompt result.
 */
typedef struct _McpPromptMessage McpPromptMessage;

/**
 * mcp_prompt_message_new:
 * @role: the message role (user or assistant)
 *
 * Creates a new prompt message.
 *
 * Returns: (transfer full): a new #McpPromptMessage
 */
McpPromptMessage *mcp_prompt_message_new (McpRole role);

/**
 * mcp_prompt_message_ref:
 * @message: a #McpPromptMessage
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpPromptMessage
 */
McpPromptMessage *mcp_prompt_message_ref (McpPromptMessage *message);

/**
 * mcp_prompt_message_unref:
 * @message: a #McpPromptMessage
 *
 * Decrements the reference count.
 */
void mcp_prompt_message_unref (McpPromptMessage *message);

/**
 * mcp_prompt_message_get_role:
 * @message: a #McpPromptMessage
 *
 * Gets the message role.
 *
 * Returns: the #McpRole
 */
McpRole mcp_prompt_message_get_role (McpPromptMessage *message);

/**
 * mcp_prompt_message_add_text:
 * @message: a #McpPromptMessage
 * @text: the text content
 *
 * Adds a text content item to the message.
 */
void mcp_prompt_message_add_text (McpPromptMessage *message,
                                  const gchar      *text);

/**
 * mcp_prompt_message_add_image:
 * @message: a #McpPromptMessage
 * @data: the base64-encoded image data
 * @mime_type: the MIME type
 *
 * Adds an image content item to the message.
 */
void mcp_prompt_message_add_image (McpPromptMessage *message,
                                   const gchar      *data,
                                   const gchar      *mime_type);

/**
 * mcp_prompt_message_add_resource:
 * @message: a #McpPromptMessage
 * @uri: the resource URI
 * @text: (nullable): the text content
 * @blob: (nullable): the base64-encoded blob content
 * @mime_type: (nullable): the MIME type
 *
 * Adds an embedded resource content item to the message.
 */
void mcp_prompt_message_add_resource (McpPromptMessage *message,
                                      const gchar      *uri,
                                      const gchar      *text,
                                      const gchar      *blob,
                                      const gchar      *mime_type);

/**
 * mcp_prompt_message_get_content:
 * @message: a #McpPromptMessage
 *
 * Gets the content items as a JSON array.
 *
 * Returns: (transfer none): the content array
 */
JsonArray *mcp_prompt_message_get_content (McpPromptMessage *message);

/**
 * mcp_prompt_message_to_json:
 * @message: a #McpPromptMessage
 *
 * Serializes the message to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_prompt_message_to_json (McpPromptMessage *message);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (McpPromptMessage, mcp_prompt_message_unref)

/**
 * McpPromptResult:
 *
 * An opaque structure representing the result of getting a prompt.
 */
typedef struct _McpPromptResult McpPromptResult;

/**
 * mcp_prompt_result_new:
 * @description: (nullable): an optional description
 *
 * Creates a new prompt result.
 *
 * Returns: (transfer full): a new #McpPromptResult
 */
McpPromptResult *mcp_prompt_result_new (const gchar *description);

/**
 * mcp_prompt_result_ref:
 * @result: a #McpPromptResult
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpPromptResult
 */
McpPromptResult *mcp_prompt_result_ref (McpPromptResult *result);

/**
 * mcp_prompt_result_unref:
 * @result: a #McpPromptResult
 *
 * Decrements the reference count.
 */
void mcp_prompt_result_unref (McpPromptResult *result);

/**
 * mcp_prompt_result_get_description:
 * @result: a #McpPromptResult
 *
 * Gets the description.
 *
 * Returns: (transfer none) (nullable): the description, or %NULL
 */
const gchar *mcp_prompt_result_get_description (McpPromptResult *result);

/**
 * mcp_prompt_result_add_message:
 * @result: a #McpPromptResult
 * @message: (transfer none): a #McpPromptMessage
 *
 * Adds a message to the result.
 */
void mcp_prompt_result_add_message (McpPromptResult  *result,
                                    McpPromptMessage *message);

/**
 * mcp_prompt_result_get_messages:
 * @result: a #McpPromptResult
 *
 * Gets the messages.
 *
 * Returns: (transfer none) (element-type McpPromptMessage): the messages
 */
GList *mcp_prompt_result_get_messages (McpPromptResult *result);

/**
 * mcp_prompt_result_to_json:
 * @result: a #McpPromptResult
 *
 * Serializes the result to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_prompt_result_to_json (McpPromptResult *result);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (McpPromptResult, mcp_prompt_result_unref)

/**
 * McpPromptProviderInterface:
 * @parent_iface: the parent interface
 * @list_prompts: lists available prompts
 * @get_prompt_async: gets a prompt asynchronously
 * @get_prompt_finish: completes an asynchronous prompt get
 *
 * The interface structure for #McpPromptProvider.
 */
struct _McpPromptProviderInterface
{
    GTypeInterface parent_iface;

    /*< public >*/

    /**
     * McpPromptProviderInterface::list_prompts:
     * @self: an #McpPromptProvider
     *
     * Lists the available prompts.
     *
     * Returns: (transfer full) (element-type McpPrompt): a list of prompts
     */
    GList *(*list_prompts) (McpPromptProvider *self);

    /**
     * McpPromptProviderInterface::get_prompt_async:
     * @self: an #McpPromptProvider
     * @name: the prompt name
     * @arguments: (nullable): the arguments as a hash table of string to string
     * @cancellable: (nullable): a #GCancellable
     * @callback: (scope async): callback to call when complete
     * @user_data: (closure): user data for @callback
     *
     * Gets a prompt asynchronously.
     */
    void (*get_prompt_async) (McpPromptProvider   *self,
                              const gchar         *name,
                              GHashTable          *arguments,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data);

    /**
     * McpPromptProviderInterface::get_prompt_finish:
     * @self: an #McpPromptProvider
     * @result: the #GAsyncResult
     * @error: (nullable): return location for a #GError
     *
     * Completes an asynchronous prompt get.
     *
     * Returns: (transfer full) (nullable): the #McpPromptResult, or %NULL on error
     */
    McpPromptResult *(*get_prompt_finish) (McpPromptProvider  *self,
                                           GAsyncResult       *result,
                                           GError            **error);

    /*< private >*/
    gpointer padding[8];
};

/**
 * mcp_prompt_provider_list_prompts:
 * @self: an #McpPromptProvider
 *
 * Lists the available prompts.
 *
 * Returns: (transfer full) (element-type McpPrompt): a list of prompts
 */
GList *mcp_prompt_provider_list_prompts (McpPromptProvider *self);

/**
 * mcp_prompt_provider_get_prompt_async:
 * @self: an #McpPromptProvider
 * @name: the prompt name
 * @arguments: (nullable): the arguments as a hash table of string to string
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Gets a prompt asynchronously.
 */
void mcp_prompt_provider_get_prompt_async (McpPromptProvider   *self,
                                           const gchar         *name,
                                           GHashTable          *arguments,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data);

/**
 * mcp_prompt_provider_get_prompt_finish:
 * @self: an #McpPromptProvider
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous prompt get.
 *
 * Returns: (transfer full) (nullable): the #McpPromptResult, or %NULL on error
 */
McpPromptResult *mcp_prompt_provider_get_prompt_finish (McpPromptProvider  *self,
                                                        GAsyncResult       *result,
                                                        GError            **error);

G_END_DECLS

#endif /* MCP_PROMPT_PROVIDER_H */
