/*
 * mcp-call.c - MCP Tool Invocation CLI
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Call a specific tool on an MCP server with JSON arguments.
 *
 * Usage:
 *   mcp-call --stdio ./server -- tool-name '{"arg": "value"}'
 *   mcp-call --http https://api.example.com/mcp -- add '{"a": 5, "b": 3}'
 *
 * Exit codes:
 *   0 - Success
 *   1 - Connection/transport error
 *   2 - Tool not found
 *   3 - Tool returned error result
 */

#include "mcp-common.h"
#include <string.h>

/* ========================================================================== */
/* Async operation context                                                     */
/* ========================================================================== */

typedef struct
{
    GMainLoop     *loop;
    McpToolResult *result;
    GError        *error;
} CallContext;

static void
on_call_tool_complete (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
    CallContext *ctx = user_data;

    ctx->result = mcp_client_call_tool_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

/* ========================================================================== */
/* Main entry point                                                            */
/* ========================================================================== */

static const gchar *description =
    "Call a tool on an MCP server with JSON arguments.\n"
    "\n"
    "The tool name and arguments are specified after '--':\n"
    "  mcp-call [OPTIONS] -- TOOL_NAME [JSON_ARGUMENTS]\n"
    "\n"
    "Examples:\n"
    "  mcp-call --stdio ./server -- echo '{\"message\": \"hello\"}'\n"
    "  mcp-call --http https://api.example.com/mcp -- add '{\"a\": 5, \"b\": 3}'\n"
    "  mcp-call -s ./calculator-server -- sqrt '{\"n\": 16}'\n"
    "\n"
    "Exit codes:\n"
    "  0  Success\n"
    "  1  Connection/transport error\n"
    "  2  Tool not found\n"
    "  3  Tool returned error result\n"
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
    g_autoptr(JsonObject) args = NULL;
    CallContext ctx;
    const gchar *tool_name;
    const gchar *json_args;
    gint exit_code;

    /* Parse command line options */
    context = g_option_context_new ("-- TOOL_NAME [JSON_ARGUMENTS]");
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

    /* Get tool name and arguments from remaining args */
    if (argc < 2)
    {
        g_printerr ("Error: Tool name required\n");
        g_printerr ("Usage: mcp-call [OPTIONS] -- TOOL_NAME [JSON_ARGUMENTS]\n");
        return MCP_CLI_EXIT_ERROR;
    }

    tool_name = argv[1];
    json_args = (argc >= 3) ? argv[2] : NULL;

    /* Parse JSON arguments if provided */
    args = mcp_cli_parse_json_args (json_args, &error);
    if (args == NULL)
    {
        g_printerr ("Error parsing arguments: %s\n", error->message);
        return MCP_CLI_EXIT_ERROR;
    }

    /* Create transport */
    transport = mcp_cli_create_transport (&error);
    if (transport == NULL)
    {
        g_printerr ("Error: %s\n", error->message);
        return MCP_CLI_EXIT_ERROR;
    }

    /* Create and connect client */
    client = mcp_client_new ("mcp-call", "1.0.0");
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

    /* Initialize call context */
    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);

    /* Call the tool */
    if (!mcp_cli_opt_quiet && !mcp_cli_opt_json)
    {
        g_print ("Calling tool '%s'...\n", tool_name);
    }

    mcp_client_call_tool_async (client, tool_name, args, NULL, on_call_tool_complete, &ctx);

    /* Wait for completion */
    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    /* Handle result */
    if (ctx.error != NULL)
    {
        /* Check if it's a "method not found" type error */
        if (g_error_matches (ctx.error, MCP_ERROR, MCP_ERROR_METHOD_NOT_FOUND))
        {
            g_printerr ("Error: Tool '%s' not found\n", tool_name);
            exit_code = MCP_CLI_EXIT_NOT_FOUND;
        }
        else
        {
            g_printerr ("Error calling tool: %s\n", ctx.error->message);
            exit_code = MCP_CLI_EXIT_ERROR;
        }
        g_error_free (ctx.error);
    }
    else if (ctx.result != NULL)
    {
        if (!mcp_cli_opt_quiet && !mcp_cli_opt_json)
        {
            g_print ("\nResult:\n");
        }

        mcp_cli_print_tool_result (ctx.result, mcp_cli_opt_json);

        if (mcp_tool_result_get_is_error (ctx.result))
        {
            exit_code = MCP_CLI_EXIT_TOOL_ERROR;
        }
        else
        {
            exit_code = MCP_CLI_EXIT_SUCCESS;
        }

        mcp_tool_result_unref (ctx.result);
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
