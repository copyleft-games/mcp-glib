/*
 * mcp-common.c - Shared utilities for MCP CLI tools
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "mcp-common.h"
#include <string.h>

/* ========================================================================== */
/* Global option variables                                                     */
/* ========================================================================== */

gchar    *mcp_cli_opt_stdio   = NULL;
gchar    *mcp_cli_opt_http    = NULL;
gchar    *mcp_cli_opt_ws      = NULL;
gchar    *mcp_cli_opt_token   = NULL;
gint      mcp_cli_opt_timeout = 30;
gboolean  mcp_cli_opt_json    = FALSE;
gboolean  mcp_cli_opt_quiet   = FALSE;
gboolean  mcp_cli_opt_license = FALSE;

/* ========================================================================== */
/* Common option entries                                                       */
/* ========================================================================== */

static GOptionEntry common_entries[] = {
    { "stdio", 's', 0, G_OPTION_ARG_STRING, &mcp_cli_opt_stdio,
      "Connect via stdio to COMMAND", "COMMAND" },
    { "http", 'H', 0, G_OPTION_ARG_STRING, &mcp_cli_opt_http,
      "Connect via HTTP to URL", "URL" },
    { "ws", 'W', 0, G_OPTION_ARG_STRING, &mcp_cli_opt_ws,
      "Connect via WebSocket to URL", "URL" },
    { "token", 't', 0, G_OPTION_ARG_STRING, &mcp_cli_opt_token,
      "Bearer token for authentication", "TOKEN" },
    { "timeout", 'T', 0, G_OPTION_ARG_INT, &mcp_cli_opt_timeout,
      "Timeout in seconds (default: 30)", "SECONDS" },
    { "json", 'j', 0, G_OPTION_ARG_NONE, &mcp_cli_opt_json,
      "Output in JSON format", NULL },
    { "quiet", 'q', 0, G_OPTION_ARG_NONE, &mcp_cli_opt_quiet,
      "Suppress non-essential output", NULL },
    { "license", 0, 0, G_OPTION_ARG_NONE, &mcp_cli_opt_license,
      "Show license information", NULL },
    { NULL }
};

GOptionEntry *
mcp_cli_get_common_options (void)
{
    return common_entries;
}

void
mcp_cli_reset_options (void)
{
    g_clear_pointer (&mcp_cli_opt_stdio, g_free);
    g_clear_pointer (&mcp_cli_opt_http, g_free);
    g_clear_pointer (&mcp_cli_opt_ws, g_free);
    g_clear_pointer (&mcp_cli_opt_token, g_free);
    mcp_cli_opt_timeout = 30;
    mcp_cli_opt_json    = FALSE;
    mcp_cli_opt_quiet   = FALSE;
    mcp_cli_opt_license = FALSE;
}

/* ========================================================================== */
/* Transport creation                                                          */
/* ========================================================================== */

McpTransport *
mcp_cli_create_transport (GError **error)
{
    gint transport_count;

    transport_count = 0;
    if (mcp_cli_opt_stdio != NULL) transport_count++;
    if (mcp_cli_opt_http != NULL) transport_count++;
    if (mcp_cli_opt_ws != NULL) transport_count++;

    if (transport_count == 0)
    {
        g_set_error (error,
                     MCP_ERROR,
                     MCP_ERROR_INVALID_PARAMS,
                     "No transport specified. Use --stdio, --http, or --ws");
        return NULL;
    }

    if (transport_count > 1)
    {
        g_set_error (error,
                     MCP_ERROR,
                     MCP_ERROR_INVALID_PARAMS,
                     "Multiple transports specified. Use only one of --stdio, --http, or --ws");
        return NULL;
    }

    /* Create stdio transport */
    if (mcp_cli_opt_stdio != NULL)
    {
        McpStdioTransport *transport;

        transport = mcp_stdio_transport_new_subprocess_simple (mcp_cli_opt_stdio, error);
        if (transport == NULL)
        {
            return NULL;
        }

        return MCP_TRANSPORT (transport);
    }

    /* Create HTTP transport */
    if (mcp_cli_opt_http != NULL)
    {
        McpHttpTransport *transport;

        transport = mcp_http_transport_new (mcp_cli_opt_http);

        if (mcp_cli_opt_token != NULL)
        {
            mcp_http_transport_set_auth_token (transport, mcp_cli_opt_token);
        }

        if (mcp_cli_opt_timeout > 0)
        {
            mcp_http_transport_set_timeout (transport, (guint)mcp_cli_opt_timeout);
        }

        return MCP_TRANSPORT (transport);
    }

    /* Create WebSocket transport */
    if (mcp_cli_opt_ws != NULL)
    {
        McpWebSocketTransport *transport;

        transport = mcp_websocket_transport_new (mcp_cli_opt_ws);

        if (mcp_cli_opt_token != NULL)
        {
            mcp_websocket_transport_set_auth_token (transport, mcp_cli_opt_token);
        }

        return MCP_TRANSPORT (transport);
    }

    /* Should not reach here */
    g_set_error (error,
                 MCP_ERROR,
                 MCP_ERROR_INTERNAL_ERROR,
                 "Internal error: no transport created");
    return NULL;
}

/* ========================================================================== */
/* Synchronous connection helpers                                              */
/* ========================================================================== */

typedef struct
{
    GMainLoop *loop;
    gboolean   success;
    GError    *error;
} SyncContext;

static void
on_connect_complete (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
    SyncContext *ctx = user_data;

    ctx->success = mcp_client_connect_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

static void
on_disconnect_complete (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
    SyncContext *ctx = user_data;

    ctx->success = mcp_client_disconnect_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

static gboolean
on_timeout (gpointer user_data)
{
    SyncContext *ctx = user_data;

    ctx->success = FALSE;
    ctx->error = g_error_new (MCP_ERROR,
                              MCP_ERROR_TIMEOUT,
                              "Connection timed out");
    g_main_loop_quit (ctx->loop);

    return G_SOURCE_REMOVE;
}

gboolean
mcp_cli_connect_sync (McpClient  *client,
                      guint       timeout_seconds,
                      GError    **error)
{
    SyncContext ctx;
    guint timeout_id;

    ctx.loop    = g_main_loop_new (NULL, FALSE);
    ctx.success = FALSE;
    ctx.error   = NULL;

    if (timeout_seconds == 0)
    {
        timeout_seconds = 30;
    }

    timeout_id = g_timeout_add_seconds (timeout_seconds, on_timeout, &ctx);

    mcp_client_connect_async (client, NULL, on_connect_complete, &ctx);

    g_main_loop_run (ctx.loop);

    g_source_remove (timeout_id);
    g_main_loop_unref (ctx.loop);

    if (ctx.error != NULL)
    {
        g_propagate_error (error, ctx.error);
        return FALSE;
    }

    return ctx.success;
}

gboolean
mcp_cli_disconnect_sync (McpClient  *client,
                         GError    **error)
{
    SyncContext ctx;

    ctx.loop    = g_main_loop_new (NULL, FALSE);
    ctx.success = FALSE;
    ctx.error   = NULL;

    mcp_client_disconnect_async (client, NULL, on_disconnect_complete, &ctx);

    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    if (ctx.error != NULL)
    {
        g_propagate_error (error, ctx.error);
        return FALSE;
    }

    return ctx.success;
}

/* ========================================================================== */
/* Output helpers                                                              */
/* ========================================================================== */

void
mcp_cli_print_license (void)
{
    g_print ("%s", MCP_CLI_LICENSE);
}

void
mcp_cli_print_server_info (McpClient *client,
                           gboolean   json_mode)
{
    McpImplementation *impl;
    const gchar *name;
    const gchar *version;
    const gchar *protocol_version;
    const gchar *instructions;

    impl = mcp_session_get_remote_implementation (MCP_SESSION (client));
    protocol_version = mcp_session_get_protocol_version (MCP_SESSION (client));
    instructions = mcp_client_get_server_instructions (client);

    if (impl != NULL)
    {
        name = mcp_implementation_get_name (impl);
        version = mcp_implementation_get_version (impl);
    }
    else
    {
        name = "unknown";
        version = "unknown";
    }

    if (json_mode)
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonGenerator) gen = json_generator_new ();
        g_autoptr(JsonNode) root = NULL;
        g_autofree gchar *json = NULL;

        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "server");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "name");
        json_builder_add_string_value (builder, name);
        json_builder_set_member_name (builder, "version");
        json_builder_add_string_value (builder, version);
        json_builder_set_member_name (builder, "protocolVersion");
        json_builder_add_string_value (builder, protocol_version ? protocol_version : "unknown");
        if (instructions != NULL)
        {
            json_builder_set_member_name (builder, "instructions");
            json_builder_add_string_value (builder, instructions);
        }
        json_builder_end_object (builder);
        json_builder_end_object (builder);

        root = json_builder_get_root (builder);
        json_generator_set_root (gen, root);
        json_generator_set_pretty (gen, TRUE);
        json = json_generator_to_data (gen, NULL);
        g_print ("%s\n", json);
    }
    else
    {
        g_print ("Server: %s v%s (protocol %s)\n",
                 name, version, protocol_version ? protocol_version : "unknown");

        if (instructions != NULL)
        {
            g_print ("Instructions: %s\n", instructions);
        }
    }
}

void
mcp_cli_print_tool_result (McpToolResult *result,
                           gboolean       json_mode)
{
    JsonArray *content;
    guint i;
    gboolean is_error;

    if (result == NULL)
    {
        if (json_mode)
        {
            g_print ("{\"error\": \"null result\"}\n");
        }
        else
        {
            g_printerr ("Error: null result\n");
        }
        return;
    }

    is_error = mcp_tool_result_get_is_error (result);
    content = mcp_tool_result_get_content (result);

    if (json_mode)
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonGenerator) gen = json_generator_new ();
        g_autoptr(JsonNode) root = NULL;
        g_autofree gchar *json = NULL;

        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "isError");
        json_builder_add_boolean_value (builder, is_error);
        json_builder_set_member_name (builder, "content");
        json_builder_begin_array (builder);
        for (i = 0; i < json_array_get_length (content); i++)
        {
            JsonNode *elem = json_array_get_element (content, i);
            json_builder_add_value (builder, json_node_copy (elem));
        }
        json_builder_end_array (builder);
        json_builder_end_object (builder);

        root = json_builder_get_root (builder);
        json_generator_set_root (gen, root);
        json_generator_set_pretty (gen, TRUE);
        json = json_generator_to_data (gen, NULL);
        g_print ("%s\n", json);
    }
    else
    {
        if (is_error)
        {
            g_printerr ("Tool returned error:\n");
        }

        for (i = 0; i < json_array_get_length (content); i++)
        {
            JsonObject *item = json_array_get_object_element (content, i);
            const gchar *type = json_object_get_string_member (item, "type");

            if (g_strcmp0 (type, "text") == 0)
            {
                g_print ("%s\n", json_object_get_string_member (item, "text"));
            }
            else if (g_strcmp0 (type, "image") == 0)
            {
                const gchar *mime = json_object_get_string_member (item, "mimeType");
                g_print ("[Image: %s, base64 data omitted]\n", mime ? mime : "unknown");
            }
            else if (g_strcmp0 (type, "resource") == 0)
            {
                JsonObject *resource = json_object_get_object_member (item, "resource");
                if (resource != NULL)
                {
                    const gchar *uri = json_object_get_string_member (resource, "uri");
                    g_print ("[Resource: %s]\n", uri ? uri : "unknown");
                }
            }
            else
            {
                g_print ("[Unknown content type: %s]\n", type ? type : "null");
            }
        }
    }
}

void
mcp_cli_print_resource_contents (GList    *contents,
                                 gboolean  json_mode)
{
    GList *l;

    if (json_mode)
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonGenerator) gen = json_generator_new ();
        g_autoptr(JsonNode) root = NULL;
        g_autofree gchar *json = NULL;

        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "contents");
        json_builder_begin_array (builder);

        for (l = contents; l != NULL; l = l->next)
        {
            McpResourceContents *c = l->data;
            const gchar *uri = mcp_resource_contents_get_uri (c);
            const gchar *mime = mcp_resource_contents_get_mime_type (c);
            const gchar *text = mcp_resource_contents_get_text (c);

            json_builder_begin_object (builder);
            json_builder_set_member_name (builder, "uri");
            json_builder_add_string_value (builder, uri ? uri : "");
            if (mime != NULL)
            {
                json_builder_set_member_name (builder, "mimeType");
                json_builder_add_string_value (builder, mime);
            }
            if (text != NULL)
            {
                json_builder_set_member_name (builder, "text");
                json_builder_add_string_value (builder, text);
            }
            else
            {
                json_builder_set_member_name (builder, "blob");
                json_builder_add_string_value (builder, "[binary data]");
            }
            json_builder_end_object (builder);
        }

        json_builder_end_array (builder);
        json_builder_end_object (builder);

        root = json_builder_get_root (builder);
        json_generator_set_root (gen, root);
        json_generator_set_pretty (gen, TRUE);
        json = json_generator_to_data (gen, NULL);
        g_print ("%s\n", json);
    }
    else
    {
        for (l = contents; l != NULL; l = l->next)
        {
            McpResourceContents *c = l->data;
            const gchar *uri = mcp_resource_contents_get_uri (c);
            const gchar *mime = mcp_resource_contents_get_mime_type (c);
            const gchar *text = mcp_resource_contents_get_text (c);

            if (g_list_length (contents) > 1)
            {
                g_print ("--- %s", uri ? uri : "unknown");
                if (mime != NULL)
                {
                    g_print (" (%s)", mime);
                }
                g_print (" ---\n");
            }

            if (text != NULL)
            {
                g_print ("%s", text);
                /* Add newline if text doesn't end with one */
                if (text[0] != '\0' && text[strlen(text) - 1] != '\n')
                {
                    g_print ("\n");
                }
            }
            else
            {
                g_print ("[Binary content - use --output to save to file]\n");
            }
        }
    }
}

void
mcp_cli_print_prompt_result (McpPromptResult *result,
                             gboolean         json_mode)
{
    GList *messages;
    GList *l;

    if (result == NULL)
    {
        if (json_mode)
        {
            g_print ("{\"error\": \"null result\"}\n");
        }
        else
        {
            g_printerr ("Error: null result\n");
        }
        return;
    }

    messages = mcp_prompt_result_get_messages (result);

    if (json_mode)
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonGenerator) gen = json_generator_new ();
        g_autoptr(JsonNode) root = NULL;
        g_autofree gchar *json = NULL;
        const gchar *description;

        description = mcp_prompt_result_get_description (result);

        json_builder_begin_object (builder);
        if (description != NULL)
        {
            json_builder_set_member_name (builder, "description");
            json_builder_add_string_value (builder, description);
        }
        json_builder_set_member_name (builder, "messages");
        json_builder_begin_array (builder);

        for (l = messages; l != NULL; l = l->next)
        {
            McpPromptMessage *msg = l->data;
            McpRole role = mcp_prompt_message_get_role (msg);
            JsonArray *content = mcp_prompt_message_get_content (msg);
            guint i;

            json_builder_begin_object (builder);
            json_builder_set_member_name (builder, "role");
            json_builder_add_string_value (builder, role == MCP_ROLE_USER ? "user" : "assistant");
            json_builder_set_member_name (builder, "content");
            json_builder_begin_array (builder);
            for (i = 0; i < json_array_get_length (content); i++)
            {
                JsonNode *elem = json_array_get_element (content, i);
                json_builder_add_value (builder, json_node_copy (elem));
            }
            json_builder_end_array (builder);
            json_builder_end_object (builder);
        }

        json_builder_end_array (builder);
        json_builder_end_object (builder);

        root = json_builder_get_root (builder);
        json_generator_set_root (gen, root);
        json_generator_set_pretty (gen, TRUE);
        json = json_generator_to_data (gen, NULL);
        g_print ("%s\n", json);
    }
    else
    {
        for (l = messages; l != NULL; l = l->next)
        {
            McpPromptMessage *msg = l->data;
            McpRole role = mcp_prompt_message_get_role (msg);
            JsonArray *content = mcp_prompt_message_get_content (msg);
            guint i;

            g_print ("[%s] ", role == MCP_ROLE_USER ? "user" : "assistant");

            for (i = 0; i < json_array_get_length (content); i++)
            {
                JsonObject *item = json_array_get_object_element (content, i);
                const gchar *type = json_object_get_string_member (item, "type");

                if (g_strcmp0 (type, "text") == 0)
                {
                    g_print ("%s", json_object_get_string_member (item, "text"));
                }
            }
            g_print ("\n");
        }
    }
}

void
mcp_cli_print_tools (GList    *tools,
                     gboolean  json_mode,
                     gboolean  verbose)
{
    GList *l;

    if (json_mode)
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonGenerator) gen = json_generator_new ();
        g_autoptr(JsonNode) root = NULL;
        g_autofree gchar *json = NULL;

        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "tools");
        json_builder_begin_array (builder);

        for (l = tools; l != NULL; l = l->next)
        {
            McpTool *tool = l->data;
            JsonNode *tool_json = mcp_tool_to_json (tool);
            json_builder_add_value (builder, tool_json);
        }

        json_builder_end_array (builder);
        json_builder_end_object (builder);

        root = json_builder_get_root (builder);
        json_generator_set_root (gen, root);
        json_generator_set_pretty (gen, TRUE);
        json = json_generator_to_data (gen, NULL);
        g_print ("%s\n", json);
    }
    else
    {
        for (l = tools; l != NULL; l = l->next)
        {
            McpTool *tool = l->data;
            const gchar *name = mcp_tool_get_name (tool);
            const gchar *desc = mcp_tool_get_description (tool);

            g_print ("  %s", name);
            if (desc != NULL)
            {
                g_print (" - %s", desc);
            }
            g_print ("\n");

            if (verbose)
            {
                JsonNode *schema = mcp_tool_get_input_schema (tool);
                if (schema != NULL)
                {
                    g_autoptr(JsonGenerator) gen = json_generator_new ();
                    g_autofree gchar *schema_str = NULL;

                    json_generator_set_root (gen, schema);
                    json_generator_set_pretty (gen, TRUE);
                    schema_str = json_generator_to_data (gen, NULL);
                    g_print ("    Input schema: %s\n", schema_str);
                }

                if (mcp_tool_get_read_only_hint (tool))
                {
                    g_print ("    [read-only]");
                }
                if (mcp_tool_get_idempotent_hint (tool))
                {
                    g_print (" [idempotent]");
                }
                if (mcp_tool_get_destructive_hint (tool))
                {
                    g_print (" [destructive]");
                }
                g_print ("\n");
            }
        }
    }
}

void
mcp_cli_print_resources (GList    *resources,
                         gboolean  json_mode)
{
    GList *l;

    if (json_mode)
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonGenerator) gen = json_generator_new ();
        g_autoptr(JsonNode) root = NULL;
        g_autofree gchar *json = NULL;

        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "resources");
        json_builder_begin_array (builder);

        for (l = resources; l != NULL; l = l->next)
        {
            McpResource *resource = l->data;
            JsonNode *res_json = mcp_resource_to_json (resource);
            json_builder_add_value (builder, res_json);
        }

        json_builder_end_array (builder);
        json_builder_end_object (builder);

        root = json_builder_get_root (builder);
        json_generator_set_root (gen, root);
        json_generator_set_pretty (gen, TRUE);
        json = json_generator_to_data (gen, NULL);
        g_print ("%s\n", json);
    }
    else
    {
        for (l = resources; l != NULL; l = l->next)
        {
            McpResource *resource = l->data;
            const gchar *uri = mcp_resource_get_uri (resource);
            const gchar *name = mcp_resource_get_name (resource);
            const gchar *mime = mcp_resource_get_mime_type (resource);

            g_print ("  %s", uri);
            if (name != NULL && g_strcmp0 (name, uri) != 0)
            {
                g_print (" (%s)", name);
            }
            if (mime != NULL)
            {
                g_print (" [%s]", mime);
            }
            g_print ("\n");
        }
    }
}

void
mcp_cli_print_resource_templates (GList    *templates,
                                  gboolean  json_mode)
{
    GList *l;

    if (json_mode)
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonGenerator) gen = json_generator_new ();
        g_autoptr(JsonNode) root = NULL;
        g_autofree gchar *json = NULL;

        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "resourceTemplates");
        json_builder_begin_array (builder);

        for (l = templates; l != NULL; l = l->next)
        {
            McpResourceTemplate *tmpl = l->data;
            JsonNode *tmpl_json = mcp_resource_template_to_json (tmpl);
            json_builder_add_value (builder, tmpl_json);
        }

        json_builder_end_array (builder);
        json_builder_end_object (builder);

        root = json_builder_get_root (builder);
        json_generator_set_root (gen, root);
        json_generator_set_pretty (gen, TRUE);
        json = json_generator_to_data (gen, NULL);
        g_print ("%s\n", json);
    }
    else
    {
        for (l = templates; l != NULL; l = l->next)
        {
            McpResourceTemplate *tmpl = l->data;
            const gchar *uri = mcp_resource_template_get_uri_template (tmpl);
            const gchar *name = mcp_resource_template_get_name (tmpl);
            const gchar *desc = mcp_resource_template_get_description (tmpl);

            g_print ("  %s", uri);
            if (name != NULL)
            {
                g_print (" (%s)", name);
            }
            g_print ("\n");
            if (desc != NULL)
            {
                g_print ("    %s\n", desc);
            }
        }
    }
}

void
mcp_cli_print_prompts (GList    *prompts,
                       gboolean  json_mode)
{
    GList *l;

    if (json_mode)
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonGenerator) gen = json_generator_new ();
        g_autoptr(JsonNode) root = NULL;
        g_autofree gchar *json = NULL;

        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "prompts");
        json_builder_begin_array (builder);

        for (l = prompts; l != NULL; l = l->next)
        {
            McpPrompt *prompt = l->data;
            JsonNode *prompt_json = mcp_prompt_to_json (prompt);
            json_builder_add_value (builder, prompt_json);
        }

        json_builder_end_array (builder);
        json_builder_end_object (builder);

        root = json_builder_get_root (builder);
        json_generator_set_root (gen, root);
        json_generator_set_pretty (gen, TRUE);
        json = json_generator_to_data (gen, NULL);
        g_print ("%s\n", json);
    }
    else
    {
        for (l = prompts; l != NULL; l = l->next)
        {
            McpPrompt *prompt = l->data;
            const gchar *name = mcp_prompt_get_name (prompt);
            const gchar *desc = mcp_prompt_get_description (prompt);
            GList *args = mcp_prompt_get_arguments (prompt);
            GList *a;

            g_print ("  %s", name);
            if (desc != NULL)
            {
                g_print (" - %s", desc);
            }
            g_print ("\n");

            for (a = args; a != NULL; a = a->next)
            {
                McpPromptArgument *arg = a->data;
                const gchar *arg_name = mcp_prompt_argument_get_name (arg);
                const gchar *arg_desc = mcp_prompt_argument_get_description (arg);
                gboolean required = mcp_prompt_argument_get_required (arg);

                g_print ("    %s%s", arg_name, required ? " (required)" : "");
                if (arg_desc != NULL)
                {
                    g_print (": %s", arg_desc);
                }
                g_print ("\n");
            }
        }
    }
}

/* ========================================================================== */
/* Argument parsing helpers                                                    */
/* ========================================================================== */

JsonObject *
mcp_cli_parse_json_args (const gchar  *json_str,
                         GError      **error)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonNode *root;
    JsonObject *obj;

    if (json_str == NULL || json_str[0] == '\0')
    {
        /* Empty args = empty object */
        return json_object_new ();
    }

    parser = json_parser_new ();

    if (!json_parser_load_from_data (parser, json_str, -1, error))
    {
        return NULL;
    }

    root = json_parser_get_root (parser);

    if (!JSON_NODE_HOLDS_OBJECT (root))
    {
        g_set_error (error,
                     MCP_ERROR,
                     MCP_ERROR_INVALID_PARAMS,
                     "Arguments must be a JSON object");
        return NULL;
    }

    obj = json_node_dup_object (root);
    return obj;
}

GHashTable *
mcp_cli_parse_prompt_args (gchar **args,
                           gint    n_args)
{
    GHashTable *table;
    gint i;

    table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    for (i = 0; i < n_args; i++)
    {
        gchar *eq;
        gchar *key;
        gchar *value;

        eq = strchr (args[i], '=');
        if (eq == NULL)
        {
            /* No '=' found, treat as key with empty value */
            g_hash_table_insert (table, g_strdup (args[i]), g_strdup (""));
        }
        else
        {
            key = g_strndup (args[i], eq - args[i]);
            value = g_strdup (eq + 1);
            g_hash_table_insert (table, key, value);
        }
    }

    return table;
}
