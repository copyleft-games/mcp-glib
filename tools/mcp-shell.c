/*
 * mcp-shell.c - Interactive MCP REPL
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Interactive shell for exploring and testing MCP servers.
 *
 * Usage:
 *   mcp-shell --stdio ./server
 *   mcp-shell --http https://api.example.com/mcp
 *
 * Commands:
 *   help                  Show help
 *   info                  Show server info
 *   tools                 List available tools
 *   resources             List available resources
 *   templates             List resource templates
 *   prompts               List available prompts
 *   call <tool> <json>    Call a tool with JSON arguments
 *   read <uri>            Read a resource
 *   get <prompt> [args]   Get a prompt (args: key=value)
 *   quit / exit           Exit the shell
 */

#include "mcp-common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

static McpClient *shell_client = NULL;
static gboolean shell_running = TRUE;

/* ========================================================================== */
/* Async helpers for synchronous commands                                      */
/* ========================================================================== */

typedef struct
{
    GMainLoop     *loop;
    gpointer       result;
    GError        *error;
} SyncOpContext;

static void
on_list_tools_sync (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    SyncOpContext *ctx = user_data;
    ctx->result = mcp_client_list_tools_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

static void
on_list_resources_sync (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
    SyncOpContext *ctx = user_data;
    ctx->result = mcp_client_list_resources_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

static void
on_list_templates_sync (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
    SyncOpContext *ctx = user_data;
    ctx->result = mcp_client_list_resource_templates_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

static void
on_list_prompts_sync (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
    SyncOpContext *ctx = user_data;
    ctx->result = mcp_client_list_prompts_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

static void
on_call_tool_sync (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    SyncOpContext *ctx = user_data;
    ctx->result = mcp_client_call_tool_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

static void
on_read_resource_sync (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
    SyncOpContext *ctx = user_data;
    ctx->result = mcp_client_read_resource_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

static void
on_get_prompt_sync (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    SyncOpContext *ctx = user_data;
    ctx->result = mcp_client_get_prompt_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

/* ========================================================================== */
/* Shell command handlers                                                      */
/* ========================================================================== */

static void
cmd_help (void)
{
    g_print ("Available commands:\n");
    g_print ("  help                  Show this help message\n");
    g_print ("  info                  Show server information\n");
    g_print ("  tools                 List available tools\n");
    g_print ("  resources             List available resources\n");
    g_print ("  templates             List resource templates\n");
    g_print ("  prompts               List available prompts\n");
    g_print ("  call <tool> <json>    Call a tool with JSON arguments\n");
    g_print ("  read <uri>            Read a resource by URI\n");
    g_print ("  get <prompt> [args]   Get a prompt (args: key=value pairs)\n");
    g_print ("  quit, exit            Exit the shell\n");
}

static void
cmd_info (void)
{
    mcp_cli_print_server_info (shell_client, FALSE);
}

static void
cmd_tools (void)
{
    SyncOpContext ctx;
    GList *tools;

    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);

    mcp_client_list_tools_async (shell_client, NULL, on_list_tools_sync, &ctx);
    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    if (ctx.error != NULL)
    {
        g_printerr ("Error: %s\n", ctx.error->message);
        g_error_free (ctx.error);
        return;
    }

    tools = ctx.result;
    if (tools != NULL)
    {
        mcp_cli_print_tools (tools, FALSE, FALSE);
        g_list_free_full (tools, g_object_unref);
    }
    else
    {
        g_print ("(no tools available)\n");
    }
}

static void
cmd_resources (void)
{
    SyncOpContext ctx;
    GList *resources;

    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);

    mcp_client_list_resources_async (shell_client, NULL, on_list_resources_sync, &ctx);
    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    if (ctx.error != NULL)
    {
        g_printerr ("Error: %s\n", ctx.error->message);
        g_error_free (ctx.error);
        return;
    }

    resources = ctx.result;
    if (resources != NULL)
    {
        mcp_cli_print_resources (resources, FALSE);
        g_list_free_full (resources, g_object_unref);
    }
    else
    {
        g_print ("(no resources available)\n");
    }
}

static void
cmd_templates (void)
{
    SyncOpContext ctx;
    GList *templates;

    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);

    mcp_client_list_resource_templates_async (shell_client, NULL, on_list_templates_sync, &ctx);
    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    if (ctx.error != NULL)
    {
        g_printerr ("Error: %s\n", ctx.error->message);
        g_error_free (ctx.error);
        return;
    }

    templates = ctx.result;
    if (templates != NULL)
    {
        mcp_cli_print_resource_templates (templates, FALSE);
        g_list_free_full (templates, g_object_unref);
    }
    else
    {
        g_print ("(no resource templates available)\n");
    }
}

static void
cmd_prompts (void)
{
    SyncOpContext ctx;
    GList *prompts;

    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);

    mcp_client_list_prompts_async (shell_client, NULL, on_list_prompts_sync, &ctx);
    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    if (ctx.error != NULL)
    {
        g_printerr ("Error: %s\n", ctx.error->message);
        g_error_free (ctx.error);
        return;
    }

    prompts = ctx.result;
    if (prompts != NULL)
    {
        mcp_cli_print_prompts (prompts, FALSE);
        g_list_free_full (prompts, g_object_unref);
    }
    else
    {
        g_print ("(no prompts available)\n");
    }
}

static void
cmd_call (const gchar *args)
{
    SyncOpContext ctx;
    g_autoptr(JsonObject) json_args = NULL;
    g_autoptr(GError) error = NULL;
    McpToolResult *result;
    gchar *tool_name;
    const gchar *json_str;
    gchar *space;

    if (args == NULL || *args == '\0')
    {
        g_printerr ("Usage: call <tool-name> <json-arguments>\n");
        return;
    }

    /* Split into tool name and JSON args */
    space = strchr (args, ' ');
    if (space != NULL)
    {
        tool_name = g_strndup (args, space - args);
        json_str = space + 1;
        while (*json_str == ' ') json_str++;
    }
    else
    {
        tool_name = g_strdup (args);
        json_str = NULL;
    }

    /* Parse JSON arguments */
    json_args = mcp_cli_parse_json_args (json_str, &error);
    if (json_args == NULL)
    {
        g_printerr ("Error parsing JSON: %s\n", error->message);
        g_free (tool_name);
        return;
    }

    /* Call the tool */
    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);

    mcp_client_call_tool_async (shell_client, tool_name, json_args, NULL, on_call_tool_sync, &ctx);
    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    g_free (tool_name);

    if (ctx.error != NULL)
    {
        g_printerr ("Error: %s\n", ctx.error->message);
        g_error_free (ctx.error);
        return;
    }

    result = ctx.result;
    if (result != NULL)
    {
        mcp_cli_print_tool_result (result, FALSE);
        mcp_tool_result_unref (result);
    }
}

static void
cmd_read (const gchar *uri)
{
    SyncOpContext ctx;
    GList *contents;

    if (uri == NULL || *uri == '\0')
    {
        g_printerr ("Usage: read <uri>\n");
        return;
    }

    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);

    mcp_client_read_resource_async (shell_client, uri, NULL, on_read_resource_sync, &ctx);
    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    if (ctx.error != NULL)
    {
        g_printerr ("Error: %s\n", ctx.error->message);
        g_error_free (ctx.error);
        return;
    }

    contents = ctx.result;
    if (contents != NULL)
    {
        mcp_cli_print_resource_contents (contents, FALSE);
        g_list_free_full (contents, (GDestroyNotify)mcp_resource_contents_unref);
    }
}

static void
cmd_get (const gchar *args)
{
    SyncOpContext ctx;
    g_autoptr(GHashTable) prompt_args = NULL;
    McpPromptResult *result;
    gchar *prompt_name;
    gchar **parts;
    gint n_parts;
    gint i;

    if (args == NULL || *args == '\0')
    {
        g_printerr ("Usage: get <prompt-name> [key=value ...]\n");
        return;
    }

    /* Split into prompt name and arguments */
    parts = g_strsplit (args, " ", -1);
    n_parts = g_strv_length (parts);

    if (n_parts == 0)
    {
        g_printerr ("Usage: get <prompt-name> [key=value ...]\n");
        g_strfreev (parts);
        return;
    }

    prompt_name = g_strdup (parts[0]);

    /* Parse key=value arguments */
    prompt_args = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    for (i = 1; i < n_parts; i++)
    {
        gchar *eq = strchr (parts[i], '=');
        if (eq != NULL)
        {
            gchar *key = g_strndup (parts[i], eq - parts[i]);
            gchar *value = g_strdup (eq + 1);
            g_hash_table_insert (prompt_args, key, value);
        }
    }

    g_strfreev (parts);

    /* Get the prompt */
    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);

    mcp_client_get_prompt_async (shell_client, prompt_name, prompt_args, NULL, on_get_prompt_sync, &ctx);
    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    g_free (prompt_name);

    if (ctx.error != NULL)
    {
        g_printerr ("Error: %s\n", ctx.error->message);
        g_error_free (ctx.error);
        return;
    }

    result = ctx.result;
    if (result != NULL)
    {
        mcp_cli_print_prompt_result (result, FALSE);
        mcp_prompt_result_unref (result);
    }
}

/* ========================================================================== */
/* Command parsing and execution                                               */
/* ========================================================================== */

static gchar *
trim_whitespace (gchar *str)
{
    gchar *end;

    /* Trim leading whitespace */
    while (isspace ((unsigned char)*str))
    {
        str++;
    }

    if (*str == '\0')
    {
        return str;
    }

    /* Trim trailing whitespace */
    end = str + strlen (str) - 1;
    while (end > str && isspace ((unsigned char)*end))
    {
        end--;
    }
    end[1] = '\0';

    return str;
}

static void
execute_command (const gchar *line)
{
    gchar *trimmed;
    gchar *cmd;
    const gchar *args;
    gchar *space;

    trimmed = g_strdup (line);
    trimmed = trim_whitespace (trimmed);

    if (*trimmed == '\0')
    {
        g_free (trimmed);
        return;
    }

    /* Split command and arguments */
    space = strchr (trimmed, ' ');
    if (space != NULL)
    {
        cmd = g_strndup (trimmed, space - trimmed);
        args = space + 1;
        while (*args == ' ') args++;
    }
    else
    {
        cmd = g_strdup (trimmed);
        args = NULL;
    }

    /* Execute command */
    if (g_strcmp0 (cmd, "help") == 0 || g_strcmp0 (cmd, "?") == 0)
    {
        cmd_help ();
    }
    else if (g_strcmp0 (cmd, "info") == 0)
    {
        cmd_info ();
    }
    else if (g_strcmp0 (cmd, "tools") == 0)
    {
        cmd_tools ();
    }
    else if (g_strcmp0 (cmd, "resources") == 0)
    {
        cmd_resources ();
    }
    else if (g_strcmp0 (cmd, "templates") == 0)
    {
        cmd_templates ();
    }
    else if (g_strcmp0 (cmd, "prompts") == 0)
    {
        cmd_prompts ();
    }
    else if (g_strcmp0 (cmd, "call") == 0)
    {
        cmd_call (args);
    }
    else if (g_strcmp0 (cmd, "read") == 0)
    {
        cmd_read (args);
    }
    else if (g_strcmp0 (cmd, "get") == 0)
    {
        cmd_get (args);
    }
    else if (g_strcmp0 (cmd, "quit") == 0 || g_strcmp0 (cmd, "exit") == 0)
    {
        shell_running = FALSE;
    }
    else
    {
        g_printerr ("Unknown command: %s\n", cmd);
        g_printerr ("Type 'help' for available commands.\n");
    }

    g_free (cmd);
    g_free (trimmed);
}

/* ========================================================================== */
/* Main entry point                                                            */
/* ========================================================================== */

static const gchar *description =
    "Interactive shell for exploring and testing MCP servers.\n"
    "\n"
    "Examples:\n"
    "  mcp-shell --stdio ./my-server\n"
    "  mcp-shell --http https://api.example.com/mcp --token \"secret\"\n"
    "\n"
    "Once connected, type 'help' to see available commands.\n"
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
    gchar *line;

    /* Parse command line options */
    context = g_option_context_new ("- Interactive MCP shell");
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
    client = mcp_client_new ("mcp-shell", "1.0.0");
    mcp_client_set_transport (client, transport);

    g_print ("Connecting...\n");

    if (!mcp_cli_connect_sync (client, mcp_cli_opt_timeout, &error))
    {
        g_printerr ("Connection failed: %s\n", error->message);
        return MCP_CLI_EXIT_ERROR;
    }

    /* Store client for command handlers */
    shell_client = client;

    /* Print welcome message */
    g_print ("\n");
    mcp_cli_print_server_info (client, FALSE);
    g_print ("\nType 'help' for available commands, 'quit' to exit.\n\n");

    /* Main REPL loop */
    using_history ();

    while (shell_running)
    {
        line = readline ("mcp> ");

        if (line == NULL)
        {
            /* EOF (Ctrl-D) */
            g_print ("\n");
            break;
        }

        if (*line != '\0')
        {
            add_history (line);
            execute_command (line);
        }

        free (line);
    }

    /* Cleanup */
    clear_history ();

    g_print ("Disconnecting...\n");
    mcp_cli_disconnect_sync (client, NULL);

    return MCP_CLI_EXIT_SUCCESS;
}
