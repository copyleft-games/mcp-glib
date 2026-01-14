/*
 * mcp-prompt.c - MCP Prompt Retrieval CLI
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Get a prompt from an MCP server with optional arguments.
 *
 * Usage:
 *   mcp-prompt --stdio ./server -- greeting name=Alice
 *   mcp-prompt --http https://api.example.com/mcp -- code-review file=main.c
 */

#include "mcp-common.h"
#include <string.h>

/* ========================================================================== */
/* Async operation context                                                     */
/* ========================================================================== */

typedef struct
{
    GMainLoop       *loop;
    McpPromptResult *result;
    GError          *error;
} PromptContext;

static void
on_get_prompt_complete (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
    PromptContext *ctx = user_data;

    ctx->result = mcp_client_get_prompt_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

/* ========================================================================== */
/* Main entry point                                                            */
/* ========================================================================== */

static const gchar *description =
    "Get a prompt from an MCP server with optional arguments.\n"
    "\n"
    "The prompt name and arguments are specified after '--':\n"
    "  mcp-prompt [OPTIONS] -- PROMPT_NAME [key=value ...]\n"
    "\n"
    "Arguments are specified as key=value pairs.\n"
    "\n"
    "Examples:\n"
    "  mcp-prompt --stdio ./server -- greeting name=Alice\n"
    "  mcp-prompt --http https://api.example.com/mcp -- code-review file=main.c lang=c\n"
    "  mcp-prompt -s ./server --json -- summarize content=\"some text\"\n"
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
    g_autoptr(GHashTable) prompt_args = NULL;
    PromptContext ctx;
    const gchar *prompt_name;
    gint exit_code;

    /* Parse command line options */
    context = g_option_context_new ("-- PROMPT_NAME [key=value ...]");
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

    /* Get prompt name from remaining args */
    if (argc < 2)
    {
        g_printerr ("Error: Prompt name required\n");
        g_printerr ("Usage: mcp-prompt [OPTIONS] -- PROMPT_NAME [key=value ...]\n");
        return MCP_CLI_EXIT_ERROR;
    }

    prompt_name = argv[1];

    /* Parse key=value arguments */
    if (argc > 2)
    {
        prompt_args = mcp_cli_parse_prompt_args (&argv[2], argc - 2);
    }
    else
    {
        prompt_args = g_hash_table_new (g_str_hash, g_str_equal);
    }

    /* Create transport */
    transport = mcp_cli_create_transport (&error);
    if (transport == NULL)
    {
        g_printerr ("Error: %s\n", error->message);
        return MCP_CLI_EXIT_ERROR;
    }

    /* Create and connect client */
    client = mcp_client_new ("mcp-prompt", "1.0.0");
    mcp_client_set_transport (client, transport);

    if (!mcp_cli_opt_quiet && !mcp_cli_opt_json)
    {
        g_print ("Connecting...\n");
    }

    if (!mcp_cli_connect_sync (client, mcp_cli_opt_timeout, &error))
    {
        g_printerr ("Connection failed: %s\n", error->message);
        return MCP_CLI_EXIT_ERROR;
    }

    /* Initialize prompt context */
    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);

    /* Get the prompt */
    if (!mcp_cli_opt_quiet && !mcp_cli_opt_json)
    {
        g_print ("Getting prompt '%s'...\n", prompt_name);
    }

    mcp_client_get_prompt_async (client, prompt_name, prompt_args, NULL, on_get_prompt_complete, &ctx);

    /* Wait for completion */
    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    /* Handle result */
    if (ctx.error != NULL)
    {
        if (g_error_matches (ctx.error, MCP_ERROR, MCP_ERROR_METHOD_NOT_FOUND))
        {
            g_printerr ("Error: Prompt '%s' not found\n", prompt_name);
            exit_code = MCP_CLI_EXIT_NOT_FOUND;
        }
        else
        {
            g_printerr ("Error getting prompt: %s\n", ctx.error->message);
            exit_code = MCP_CLI_EXIT_ERROR;
        }
        g_error_free (ctx.error);
    }
    else if (ctx.result != NULL)
    {
        if (!mcp_cli_opt_quiet && !mcp_cli_opt_json)
        {
            g_print ("\n");
        }

        mcp_cli_print_prompt_result (ctx.result, mcp_cli_opt_json);
        mcp_prompt_result_unref (ctx.result);
        exit_code = MCP_CLI_EXIT_SUCCESS;
    }
    else
    {
        g_printerr ("Error: No result received\n");
        exit_code = MCP_CLI_EXIT_ERROR;
    }

    /* Disconnect */
    mcp_cli_disconnect_sync (client, NULL);

    return exit_code;
}
