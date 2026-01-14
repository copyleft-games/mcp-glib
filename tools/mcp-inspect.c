/*
 * mcp-inspect.c - MCP Server Inspection Tool
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Connect to an MCP server and display its capabilities, tools,
 * resources, and prompts.
 *
 * Usage:
 *   mcp-inspect --stdio ./my-server
 *   mcp-inspect --http https://api.example.com/mcp --token "secret"
 *   mcp-inspect --ws wss://example.com/mcp --json
 */

#include "mcp-common.h"

/* ========================================================================== */
/* Async operation context                                                     */
/* ========================================================================== */

typedef struct
{
    GMainLoop *loop;
    McpClient *client;
    GList     *tools;
    GList     *resources;
    GList     *templates;
    GList     *prompts;
    GError    *error;
    gint       pending;
} InspectContext;

static void
on_list_tools_complete (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
    InspectContext *ctx = user_data;

    ctx->tools = mcp_client_list_tools_finish (MCP_CLIENT (source), result, &ctx->error);
    ctx->pending--;

    if (ctx->pending == 0)
    {
        g_main_loop_quit (ctx->loop);
    }
}

static void
on_list_resources_complete (GObject      *source,
                            GAsyncResult *result,
                            gpointer      user_data)
{
    InspectContext *ctx = user_data;

    ctx->resources = mcp_client_list_resources_finish (MCP_CLIENT (source), result, NULL);
    ctx->pending--;

    if (ctx->pending == 0)
    {
        g_main_loop_quit (ctx->loop);
    }
}

static void
on_list_templates_complete (GObject      *source,
                            GAsyncResult *result,
                            gpointer      user_data)
{
    InspectContext *ctx = user_data;

    ctx->templates = mcp_client_list_resource_templates_finish (MCP_CLIENT (source), result, NULL);
    ctx->pending--;

    if (ctx->pending == 0)
    {
        g_main_loop_quit (ctx->loop);
    }
}

static void
on_list_prompts_complete (GObject      *source,
                          GAsyncResult *result,
                          gpointer      user_data)
{
    InspectContext *ctx = user_data;

    ctx->prompts = mcp_client_list_prompts_finish (MCP_CLIENT (source), result, NULL);
    ctx->pending--;

    if (ctx->pending == 0)
    {
        g_main_loop_quit (ctx->loop);
    }
}

/* ========================================================================== */
/* Main entry point                                                            */
/* ========================================================================== */

static const gchar *description =
    "Connect to an MCP server and display its capabilities, tools, resources,\n"
    "and prompts.\n"
    "\n"
    "Examples:\n"
    "  mcp-inspect --stdio ./my-server\n"
    "  mcp-inspect --http https://api.example.com/mcp --token \"secret\"\n"
    "  mcp-inspect -s './server --debug' --json\n"
    "\n"
    "Report bugs to: " MCP_CLI_BUG_URL;

int
main (int    argc,
      char **argv)
{
    g_autoptr(GOptionContext) context = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(McpClient) client = NULL;
    g_autoptr(McpTransport) transport = NULL;
    InspectContext ctx;
    McpServerCapabilities *caps;

    /* Parse command line options */
    context = g_option_context_new ("- Inspect MCP server capabilities");
    g_option_context_set_description (context, description);
    g_option_context_add_main_entries (context, mcp_cli_get_common_options (), NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_printerr ("Option parsing failed: %s\n", error->message);
        return MCP_CLI_EXIT_ERROR;
    }

    /* Handle --license */
    if (mcp_cli_opt_license)
    {
        mcp_cli_print_license ();
        return MCP_CLI_EXIT_SUCCESS;
    }

    /* Create transport */
    transport = mcp_cli_create_transport (&error);
    if (transport == NULL)
    {
        g_printerr ("Error: %s\n", error->message);
        return MCP_CLI_EXIT_ERROR;
    }

    /* Create and connect client */
    client = mcp_client_new ("mcp-inspect", "1.0.0");
    mcp_client_set_transport (client, transport);

    if (!mcp_cli_opt_quiet)
    {
        g_print ("Connecting...\n");
    }

    if (!mcp_cli_connect_sync (client, mcp_cli_opt_timeout, &error))
    {
        g_printerr ("Connection failed: %s\n", error->message);
        return MCP_CLI_EXIT_ERROR;
    }

    /* Print server info */
    if (!mcp_cli_opt_quiet && !mcp_cli_opt_json)
    {
        g_print ("\n");
    }
    mcp_cli_print_server_info (client, mcp_cli_opt_json);

    /* Print capabilities */
    caps = mcp_client_get_server_capabilities (client);
    if (caps != NULL && !mcp_cli_opt_json)
    {
        gboolean has_tools, has_resources, has_prompts, has_logging;

        has_tools = mcp_server_capabilities_get_tools (caps);
        has_resources = mcp_server_capabilities_get_resources (caps);
        has_prompts = mcp_server_capabilities_get_prompts (caps);
        has_logging = mcp_server_capabilities_get_logging (caps);

        if (!mcp_cli_opt_quiet)
        {
            g_print ("Capabilities:");
            if (has_tools) g_print (" tools");
            if (has_resources) g_print (" resources");
            if (has_prompts) g_print (" prompts");
            if (has_logging) g_print (" logging");
            g_print ("\n\n");
        }
    }

    /* Initialize context for async operations */
    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);
    ctx.client = client;
    ctx.pending = 4;

    /* Start all list operations in parallel */
    mcp_client_list_tools_async (client, NULL, on_list_tools_complete, &ctx);
    mcp_client_list_resources_async (client, NULL, on_list_resources_complete, &ctx);
    mcp_client_list_resource_templates_async (client, NULL, on_list_templates_complete, &ctx);
    mcp_client_list_prompts_async (client, NULL, on_list_prompts_complete, &ctx);

    /* Wait for all operations to complete */
    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    if (ctx.error != NULL)
    {
        g_printerr ("Error listing tools: %s\n", ctx.error->message);
        g_error_free (ctx.error);
        return MCP_CLI_EXIT_ERROR;
    }

    /* Print results */
    if (mcp_cli_opt_json)
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonGenerator) gen = json_generator_new ();
        g_autoptr(JsonNode) root = NULL;
        g_autofree gchar *json = NULL;
        McpImplementation *impl;
        GList *l;

        impl = mcp_session_get_remote_implementation (MCP_SESSION (client));

        json_builder_begin_object (builder);

        /* Server info */
        json_builder_set_member_name (builder, "server");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "name");
        json_builder_add_string_value (builder,
            impl ? mcp_implementation_get_name (impl) : "unknown");
        json_builder_set_member_name (builder, "version");
        json_builder_add_string_value (builder,
            impl ? mcp_implementation_get_version (impl) : "unknown");
        json_builder_set_member_name (builder, "protocolVersion");
        json_builder_add_string_value (builder,
            mcp_session_get_protocol_version (MCP_SESSION (client)) ?: "unknown");
        json_builder_end_object (builder);

        /* Tools */
        json_builder_set_member_name (builder, "tools");
        json_builder_begin_array (builder);
        for (l = ctx.tools; l != NULL; l = l->next)
        {
            McpTool *tool = l->data;
            JsonNode *tool_json = mcp_tool_to_json (tool);
            json_builder_add_value (builder, tool_json);
        }
        json_builder_end_array (builder);

        /* Resources */
        json_builder_set_member_name (builder, "resources");
        json_builder_begin_array (builder);
        for (l = ctx.resources; l != NULL; l = l->next)
        {
            McpResource *res = l->data;
            JsonNode *res_json = mcp_resource_to_json (res);
            json_builder_add_value (builder, res_json);
        }
        json_builder_end_array (builder);

        /* Resource templates */
        json_builder_set_member_name (builder, "resourceTemplates");
        json_builder_begin_array (builder);
        for (l = ctx.templates; l != NULL; l = l->next)
        {
            McpResourceTemplate *tmpl = l->data;
            JsonNode *tmpl_json = mcp_resource_template_to_json (tmpl);
            json_builder_add_value (builder, tmpl_json);
        }
        json_builder_end_array (builder);

        /* Prompts */
        json_builder_set_member_name (builder, "prompts");
        json_builder_begin_array (builder);
        for (l = ctx.prompts; l != NULL; l = l->next)
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
        /* Human-readable output */
        g_print ("Tools (%u):\n", g_list_length (ctx.tools));
        if (ctx.tools != NULL)
        {
            mcp_cli_print_tools (ctx.tools, FALSE, !mcp_cli_opt_quiet);
        }
        else
        {
            g_print ("  (none)\n");
        }

        g_print ("\nResources (%u):\n", g_list_length (ctx.resources));
        if (ctx.resources != NULL)
        {
            mcp_cli_print_resources (ctx.resources, FALSE);
        }
        else
        {
            g_print ("  (none)\n");
        }

        if (ctx.templates != NULL)
        {
            g_print ("\nResource Templates (%u):\n", g_list_length (ctx.templates));
            mcp_cli_print_resource_templates (ctx.templates, FALSE);
        }

        g_print ("\nPrompts (%u):\n", g_list_length (ctx.prompts));
        if (ctx.prompts != NULL)
        {
            mcp_cli_print_prompts (ctx.prompts, FALSE);
        }
        else
        {
            g_print ("  (none)\n");
        }
    }

    /* Cleanup */
    g_list_free_full (ctx.tools, g_object_unref);
    g_list_free_full (ctx.resources, g_object_unref);
    g_list_free_full (ctx.templates, g_object_unref);
    g_list_free_full (ctx.prompts, g_object_unref);

    /* Disconnect */
    mcp_cli_disconnect_sync (client, NULL);

    return MCP_CLI_EXIT_SUCCESS;
}
