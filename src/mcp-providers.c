/*
 * mcp-providers.c - Provider interfaces and result types implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp-tool-provider.h"
#include "mcp-resource-provider.h"
#include "mcp-prompt-provider.h"
#include "mcp-error.h"
#undef MCP_COMPILATION

/* ========================================================================== */
/* McpToolResult                                                              */
/* ========================================================================== */

struct _McpToolResult
{
    gint       ref_count;
    gboolean   is_error;
    JsonArray *content;
};

McpToolResult *
mcp_tool_result_new (gboolean is_error)
{
    McpToolResult *result;

    result = g_new0 (McpToolResult, 1);
    result->ref_count = 1;
    result->is_error = is_error;
    result->content = json_array_new ();

    return result;
}

McpToolResult *
mcp_tool_result_ref (McpToolResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);

    result->ref_count++;
    return result;
}

void
mcp_tool_result_unref (McpToolResult *result)
{
    g_return_if_fail (result != NULL);

    if (--result->ref_count == 0)
    {
        json_array_unref (result->content);
        g_free (result);
    }
}

gboolean
mcp_tool_result_get_is_error (McpToolResult *result)
{
    g_return_val_if_fail (result != NULL, FALSE);
    return result->is_error;
}

void
mcp_tool_result_add_text (McpToolResult *result,
                          const gchar   *text)
{
    JsonObject *item;

    g_return_if_fail (result != NULL);
    g_return_if_fail (text != NULL);

    item = json_object_new ();
    json_object_set_string_member (item, "type", "text");
    json_object_set_string_member (item, "text", text);
    json_array_add_object_element (result->content, item);
}

void
mcp_tool_result_add_image (McpToolResult *result,
                           const gchar   *data,
                           const gchar   *mime_type)
{
    JsonObject *item;

    g_return_if_fail (result != NULL);
    g_return_if_fail (data != NULL);
    g_return_if_fail (mime_type != NULL);

    item = json_object_new ();
    json_object_set_string_member (item, "type", "image");
    json_object_set_string_member (item, "data", data);
    json_object_set_string_member (item, "mimeType", mime_type);
    json_array_add_object_element (result->content, item);
}

void
mcp_tool_result_add_resource (McpToolResult *result,
                              const gchar   *uri,
                              const gchar   *text,
                              const gchar   *blob,
                              const gchar   *mime_type)
{
    JsonObject *item;
    JsonObject *resource;

    g_return_if_fail (result != NULL);
    g_return_if_fail (uri != NULL);

    item = json_object_new ();
    json_object_set_string_member (item, "type", "resource");

    resource = json_object_new ();
    json_object_set_string_member (resource, "uri", uri);
    if (text != NULL)
    {
        json_object_set_string_member (resource, "text", text);
    }
    if (blob != NULL)
    {
        json_object_set_string_member (resource, "blob", blob);
    }
    if (mime_type != NULL)
    {
        json_object_set_string_member (resource, "mimeType", mime_type);
    }
    json_object_set_object_member (item, "resource", resource);

    json_array_add_object_element (result->content, item);
}

JsonArray *
mcp_tool_result_get_content (McpToolResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);
    return result->content;
}

JsonNode *
mcp_tool_result_to_json (McpToolResult *result)
{
    JsonBuilder *builder;
    JsonNode *node;
    JsonNode *arr_node;

    g_return_val_if_fail (result != NULL, NULL);

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "content");

    /* Create a node that wraps our content array */
    /* json_node_set_array takes its own ref, so don't add an extra one */
    arr_node = json_node_new (JSON_NODE_ARRAY);
    json_node_set_array (arr_node, result->content);
    json_builder_add_value (builder, arr_node);

    if (result->is_error)
    {
        json_builder_set_member_name (builder, "isError");
        json_builder_add_boolean_value (builder, TRUE);
    }

    json_builder_end_object (builder);
    node = json_builder_get_root (builder);
    g_object_unref (builder);

    return node;
}

/* ========================================================================== */
/* McpResourceContents                                                        */
/* ========================================================================== */

struct _McpResourceContents
{
    gint     ref_count;
    gchar   *uri;
    gchar   *mime_type;
    gchar   *text;
    gchar   *blob;
    gboolean is_text;
};

McpResourceContents *
mcp_resource_contents_new_text (const gchar *uri,
                                const gchar *text,
                                const gchar *mime_type)
{
    McpResourceContents *contents;

    g_return_val_if_fail (uri != NULL, NULL);
    g_return_val_if_fail (text != NULL, NULL);

    contents = g_new0 (McpResourceContents, 1);
    contents->ref_count = 1;
    contents->uri = g_strdup (uri);
    contents->text = g_strdup (text);
    contents->mime_type = g_strdup (mime_type);
    contents->is_text = TRUE;

    return contents;
}

McpResourceContents *
mcp_resource_contents_new_blob (const gchar *uri,
                                const gchar *blob,
                                const gchar *mime_type)
{
    McpResourceContents *contents;

    g_return_val_if_fail (uri != NULL, NULL);
    g_return_val_if_fail (blob != NULL, NULL);

    contents = g_new0 (McpResourceContents, 1);
    contents->ref_count = 1;
    contents->uri = g_strdup (uri);
    contents->blob = g_strdup (blob);
    contents->mime_type = g_strdup (mime_type);
    contents->is_text = FALSE;

    return contents;
}

McpResourceContents *
mcp_resource_contents_ref (McpResourceContents *contents)
{
    g_return_val_if_fail (contents != NULL, NULL);

    contents->ref_count++;
    return contents;
}

void
mcp_resource_contents_unref (McpResourceContents *contents)
{
    g_return_if_fail (contents != NULL);

    if (--contents->ref_count == 0)
    {
        g_free (contents->uri);
        g_free (contents->mime_type);
        g_free (contents->text);
        g_free (contents->blob);
        g_free (contents);
    }
}

const gchar *
mcp_resource_contents_get_uri (McpResourceContents *contents)
{
    g_return_val_if_fail (contents != NULL, NULL);
    return contents->uri;
}

const gchar *
mcp_resource_contents_get_mime_type (McpResourceContents *contents)
{
    g_return_val_if_fail (contents != NULL, NULL);
    return contents->mime_type;
}

const gchar *
mcp_resource_contents_get_text (McpResourceContents *contents)
{
    g_return_val_if_fail (contents != NULL, NULL);
    return contents->text;
}

const gchar *
mcp_resource_contents_get_blob (McpResourceContents *contents)
{
    g_return_val_if_fail (contents != NULL, NULL);
    return contents->blob;
}

gboolean
mcp_resource_contents_is_text (McpResourceContents *contents)
{
    g_return_val_if_fail (contents != NULL, FALSE);
    return contents->is_text;
}

JsonNode *
mcp_resource_contents_to_json (McpResourceContents *contents)
{
    JsonBuilder *builder;
    JsonNode *node;

    g_return_val_if_fail (contents != NULL, NULL);

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "uri");
    json_builder_add_string_value (builder, contents->uri);

    if (contents->mime_type != NULL)
    {
        json_builder_set_member_name (builder, "mimeType");
        json_builder_add_string_value (builder, contents->mime_type);
    }

    if (contents->is_text)
    {
        json_builder_set_member_name (builder, "text");
        json_builder_add_string_value (builder, contents->text);
    }
    else
    {
        json_builder_set_member_name (builder, "blob");
        json_builder_add_string_value (builder, contents->blob);
    }

    json_builder_end_object (builder);
    node = json_builder_get_root (builder);
    g_object_unref (builder);

    return node;
}

/* ========================================================================== */
/* McpPromptMessage                                                           */
/* ========================================================================== */

struct _McpPromptMessage
{
    gint       ref_count;
    McpRole    role;
    JsonArray *content;
};

McpPromptMessage *
mcp_prompt_message_new (McpRole role)
{
    McpPromptMessage *message;

    message = g_new0 (McpPromptMessage, 1);
    message->ref_count = 1;
    message->role = role;
    message->content = json_array_new ();

    return message;
}

McpPromptMessage *
mcp_prompt_message_ref (McpPromptMessage *message)
{
    g_return_val_if_fail (message != NULL, NULL);

    message->ref_count++;
    return message;
}

void
mcp_prompt_message_unref (McpPromptMessage *message)
{
    g_return_if_fail (message != NULL);

    if (--message->ref_count == 0)
    {
        json_array_unref (message->content);
        g_free (message);
    }
}

McpRole
mcp_prompt_message_get_role (McpPromptMessage *message)
{
    g_return_val_if_fail (message != NULL, MCP_ROLE_USER);
    return message->role;
}

void
mcp_prompt_message_add_text (McpPromptMessage *message,
                             const gchar      *text)
{
    JsonObject *item;

    g_return_if_fail (message != NULL);
    g_return_if_fail (text != NULL);

    item = json_object_new ();
    json_object_set_string_member (item, "type", "text");
    json_object_set_string_member (item, "text", text);
    json_array_add_object_element (message->content, item);
}

void
mcp_prompt_message_add_image (McpPromptMessage *message,
                              const gchar      *data,
                              const gchar      *mime_type)
{
    JsonObject *item;

    g_return_if_fail (message != NULL);
    g_return_if_fail (data != NULL);
    g_return_if_fail (mime_type != NULL);

    item = json_object_new ();
    json_object_set_string_member (item, "type", "image");
    json_object_set_string_member (item, "data", data);
    json_object_set_string_member (item, "mimeType", mime_type);
    json_array_add_object_element (message->content, item);
}

void
mcp_prompt_message_add_resource (McpPromptMessage *message,
                                 const gchar      *uri,
                                 const gchar      *text,
                                 const gchar      *blob,
                                 const gchar      *mime_type)
{
    JsonObject *item;
    JsonObject *resource;

    g_return_if_fail (message != NULL);
    g_return_if_fail (uri != NULL);

    item = json_object_new ();
    json_object_set_string_member (item, "type", "resource");

    resource = json_object_new ();
    json_object_set_string_member (resource, "uri", uri);
    if (text != NULL)
    {
        json_object_set_string_member (resource, "text", text);
    }
    if (blob != NULL)
    {
        json_object_set_string_member (resource, "blob", blob);
    }
    if (mime_type != NULL)
    {
        json_object_set_string_member (resource, "mimeType", mime_type);
    }
    json_object_set_object_member (item, "resource", resource);

    json_array_add_object_element (message->content, item);
}

JsonArray *
mcp_prompt_message_get_content (McpPromptMessage *message)
{
    g_return_val_if_fail (message != NULL, NULL);
    return message->content;
}

JsonNode *
mcp_prompt_message_to_json (McpPromptMessage *message)
{
    JsonBuilder *builder;
    JsonNode *node;
    const gchar *role_str;

    g_return_val_if_fail (message != NULL, NULL);

    role_str = mcp_role_to_string (message->role);

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "role");
    json_builder_add_string_value (builder, role_str);

    json_builder_set_member_name (builder, "content");
    {
        JsonNode *arr = json_node_new (JSON_NODE_ARRAY);
        json_node_set_array (arr, message->content);
        json_builder_add_value (builder, arr);
    }

    json_builder_end_object (builder);
    node = json_builder_get_root (builder);
    g_object_unref (builder);

    return node;
}

/* ========================================================================== */
/* McpPromptResult                                                            */
/* ========================================================================== */

struct _McpPromptResult
{
    gint    ref_count;
    gchar  *description;
    GList  *messages;
};

McpPromptResult *
mcp_prompt_result_new (const gchar *description)
{
    McpPromptResult *result;

    result = g_new0 (McpPromptResult, 1);
    result->ref_count = 1;
    result->description = g_strdup (description);

    return result;
}

McpPromptResult *
mcp_prompt_result_ref (McpPromptResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);

    result->ref_count++;
    return result;
}

void
mcp_prompt_result_unref (McpPromptResult *result)
{
    g_return_if_fail (result != NULL);

    if (--result->ref_count == 0)
    {
        g_free (result->description);
        g_list_free_full (result->messages,
                          (GDestroyNotify) mcp_prompt_message_unref);
        g_free (result);
    }
}

const gchar *
mcp_prompt_result_get_description (McpPromptResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);
    return result->description;
}

void
mcp_prompt_result_add_message (McpPromptResult  *result,
                               McpPromptMessage *message)
{
    g_return_if_fail (result != NULL);
    g_return_if_fail (message != NULL);

    result->messages = g_list_append (result->messages,
                                      mcp_prompt_message_ref (message));
}

GList *
mcp_prompt_result_get_messages (McpPromptResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);
    return result->messages;
}

JsonNode *
mcp_prompt_result_to_json (McpPromptResult *result)
{
    JsonBuilder *builder;
    JsonNode *node;
    GList *l;

    g_return_val_if_fail (result != NULL, NULL);

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    if (result->description != NULL)
    {
        json_builder_set_member_name (builder, "description");
        json_builder_add_string_value (builder, result->description);
    }

    json_builder_set_member_name (builder, "messages");
    json_builder_begin_array (builder);
    for (l = result->messages; l != NULL; l = l->next)
    {
        McpPromptMessage *msg = l->data;
        g_autoptr(JsonNode) msg_node = mcp_prompt_message_to_json (msg);
        json_builder_add_value (builder, g_steal_pointer (&msg_node));
    }
    json_builder_end_array (builder);

    json_builder_end_object (builder);
    node = json_builder_get_root (builder);
    g_object_unref (builder);

    return node;
}

/* ========================================================================== */
/* McpToolProvider Interface                                                  */
/* ========================================================================== */

G_DEFINE_INTERFACE (McpToolProvider, mcp_tool_provider, G_TYPE_OBJECT)

static void
mcp_tool_provider_default_init (McpToolProviderInterface *iface)
{
    /* Default implementations can be set here if needed */
}

GList *
mcp_tool_provider_list_tools (McpToolProvider *self)
{
    McpToolProviderInterface *iface;

    g_return_val_if_fail (MCP_IS_TOOL_PROVIDER (self), NULL);

    iface = MCP_TOOL_PROVIDER_GET_IFACE (self);
    if (iface->list_tools != NULL)
    {
        return iface->list_tools (self);
    }

    return NULL;
}

void
mcp_tool_provider_call_tool_async (McpToolProvider     *self,
                                   const gchar         *name,
                                   JsonObject          *arguments,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
    McpToolProviderInterface *iface;

    g_return_if_fail (MCP_IS_TOOL_PROVIDER (self));
    g_return_if_fail (name != NULL);

    iface = MCP_TOOL_PROVIDER_GET_IFACE (self);
    if (iface->call_tool_async != NULL)
    {
        iface->call_tool_async (self, name, arguments, cancellable, callback, user_data);
    }
    else
    {
        g_task_report_new_error (self, callback, user_data,
                                 mcp_tool_provider_call_tool_async,
                                 MCP_ERROR, MCP_ERROR_METHOD_NOT_FOUND,
                                 "call_tool not implemented");
    }
}

McpToolResult *
mcp_tool_provider_call_tool_finish (McpToolProvider  *self,
                                    GAsyncResult     *result,
                                    GError          **error)
{
    McpToolProviderInterface *iface;

    g_return_val_if_fail (MCP_IS_TOOL_PROVIDER (self), NULL);
    g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

    iface = MCP_TOOL_PROVIDER_GET_IFACE (self);
    if (iface->call_tool_finish != NULL)
    {
        return iface->call_tool_finish (self, result, error);
    }

    return g_task_propagate_pointer (G_TASK (result), error);
}

/* ========================================================================== */
/* McpResourceProvider Interface                                              */
/* ========================================================================== */

G_DEFINE_INTERFACE (McpResourceProvider, mcp_resource_provider, G_TYPE_OBJECT)

static void
mcp_resource_provider_default_init (McpResourceProviderInterface *iface)
{
}

GList *
mcp_resource_provider_list_resources (McpResourceProvider *self)
{
    McpResourceProviderInterface *iface;

    g_return_val_if_fail (MCP_IS_RESOURCE_PROVIDER (self), NULL);

    iface = MCP_RESOURCE_PROVIDER_GET_IFACE (self);
    if (iface->list_resources != NULL)
    {
        return iface->list_resources (self);
    }

    return NULL;
}

GList *
mcp_resource_provider_list_resource_templates (McpResourceProvider *self)
{
    McpResourceProviderInterface *iface;

    g_return_val_if_fail (MCP_IS_RESOURCE_PROVIDER (self), NULL);

    iface = MCP_RESOURCE_PROVIDER_GET_IFACE (self);
    if (iface->list_resource_templates != NULL)
    {
        return iface->list_resource_templates (self);
    }

    return NULL;
}

void
mcp_resource_provider_read_resource_async (McpResourceProvider *self,
                                           const gchar         *uri,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
    McpResourceProviderInterface *iface;

    g_return_if_fail (MCP_IS_RESOURCE_PROVIDER (self));
    g_return_if_fail (uri != NULL);

    iface = MCP_RESOURCE_PROVIDER_GET_IFACE (self);
    if (iface->read_resource_async != NULL)
    {
        iface->read_resource_async (self, uri, cancellable, callback, user_data);
    }
    else
    {
        g_task_report_new_error (self, callback, user_data,
                                 mcp_resource_provider_read_resource_async,
                                 MCP_ERROR, MCP_ERROR_METHOD_NOT_FOUND,
                                 "read_resource not implemented");
    }
}

GList *
mcp_resource_provider_read_resource_finish (McpResourceProvider  *self,
                                            GAsyncResult         *result,
                                            GError              **error)
{
    McpResourceProviderInterface *iface;

    g_return_val_if_fail (MCP_IS_RESOURCE_PROVIDER (self), NULL);
    g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

    iface = MCP_RESOURCE_PROVIDER_GET_IFACE (self);
    if (iface->read_resource_finish != NULL)
    {
        return iface->read_resource_finish (self, result, error);
    }

    return g_task_propagate_pointer (G_TASK (result), error);
}

gboolean
mcp_resource_provider_subscribe (McpResourceProvider  *self,
                                 const gchar          *uri,
                                 GError              **error)
{
    McpResourceProviderInterface *iface;

    g_return_val_if_fail (MCP_IS_RESOURCE_PROVIDER (self), FALSE);
    g_return_val_if_fail (uri != NULL, FALSE);

    iface = MCP_RESOURCE_PROVIDER_GET_IFACE (self);
    if (iface->subscribe != NULL)
    {
        return iface->subscribe (self, uri, error);
    }

    return TRUE; /* Default: no-op success */
}

gboolean
mcp_resource_provider_unsubscribe (McpResourceProvider  *self,
                                   const gchar          *uri,
                                   GError              **error)
{
    McpResourceProviderInterface *iface;

    g_return_val_if_fail (MCP_IS_RESOURCE_PROVIDER (self), FALSE);
    g_return_val_if_fail (uri != NULL, FALSE);

    iface = MCP_RESOURCE_PROVIDER_GET_IFACE (self);
    if (iface->unsubscribe != NULL)
    {
        return iface->unsubscribe (self, uri, error);
    }

    return TRUE; /* Default: no-op success */
}

/* ========================================================================== */
/* McpPromptProvider Interface                                                */
/* ========================================================================== */

G_DEFINE_INTERFACE (McpPromptProvider, mcp_prompt_provider, G_TYPE_OBJECT)

static void
mcp_prompt_provider_default_init (McpPromptProviderInterface *iface)
{
}

GList *
mcp_prompt_provider_list_prompts (McpPromptProvider *self)
{
    McpPromptProviderInterface *iface;

    g_return_val_if_fail (MCP_IS_PROMPT_PROVIDER (self), NULL);

    iface = MCP_PROMPT_PROVIDER_GET_IFACE (self);
    if (iface->list_prompts != NULL)
    {
        return iface->list_prompts (self);
    }

    return NULL;
}

void
mcp_prompt_provider_get_prompt_async (McpPromptProvider   *self,
                                      const gchar         *name,
                                      GHashTable          *arguments,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
    McpPromptProviderInterface *iface;

    g_return_if_fail (MCP_IS_PROMPT_PROVIDER (self));
    g_return_if_fail (name != NULL);

    iface = MCP_PROMPT_PROVIDER_GET_IFACE (self);
    if (iface->get_prompt_async != NULL)
    {
        iface->get_prompt_async (self, name, arguments, cancellable, callback, user_data);
    }
    else
    {
        g_task_report_new_error (self, callback, user_data,
                                 mcp_prompt_provider_get_prompt_async,
                                 MCP_ERROR, MCP_ERROR_METHOD_NOT_FOUND,
                                 "get_prompt not implemented");
    }
}

McpPromptResult *
mcp_prompt_provider_get_prompt_finish (McpPromptProvider  *self,
                                       GAsyncResult       *result,
                                       GError            **error)
{
    McpPromptProviderInterface *iface;

    g_return_val_if_fail (MCP_IS_PROMPT_PROVIDER (self), NULL);
    g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

    iface = MCP_PROMPT_PROVIDER_GET_IFACE (self);
    if (iface->get_prompt_finish != NULL)
    {
        return iface->get_prompt_finish (self, result, error);
    }

    return g_task_propagate_pointer (G_TASK (result), error);
}
