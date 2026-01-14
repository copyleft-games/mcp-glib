/*
 * mcp-read.c - MCP Resource Reader CLI
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Read a resource from an MCP server by URI.
 *
 * Usage:
 *   mcp-read --stdio ./server -- file:///path/to/file
 *   mcp-read --http https://api.example.com/mcp -- config://app/settings
 *   mcp-read -s ./fs-server --output data.bin -- binary://file
 */

#include "mcp-common.h"
#include <string.h>

/* Tool-specific option */
static gchar *opt_output = NULL;

static GOptionEntry read_entries[] = {
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &opt_output,
      "Write content to FILE (for binary)", "FILE" },
    { NULL }
};

/* ========================================================================== */
/* Async operation context                                                     */
/* ========================================================================== */

typedef struct
{
    GMainLoop *loop;
    GList     *contents;
    GError    *error;
} ReadContext;

static void
on_read_resource_complete (GObject      *source,
                           GAsyncResult *result,
                           gpointer      user_data)
{
    ReadContext *ctx = user_data;

    ctx->contents = mcp_client_read_resource_finish (MCP_CLIENT (source), result, &ctx->error);
    g_main_loop_quit (ctx->loop);
}

/* ========================================================================== */
/* Main entry point                                                            */
/* ========================================================================== */

static const gchar *description =
    "Read a resource from an MCP server by URI.\n"
    "\n"
    "The resource URI is specified after '--':\n"
    "  mcp-read [OPTIONS] -- URI\n"
    "\n"
    "Examples:\n"
    "  mcp-read --stdio ./server -- file:///etc/hostname\n"
    "  mcp-read --http https://api.example.com/mcp -- config://app/settings\n"
    "  mcp-read -s ./fs-server --output data.bin -- binary://file\n"
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
    ReadContext ctx;
    const gchar *uri;
    gint exit_code;

    /* Parse command line options */
    context = g_option_context_new ("-- URI");
    g_option_context_set_description (context, description);
    g_option_context_add_main_entries (context, mcp_cli_get_common_options (), NULL);
    g_option_context_add_main_entries (context, read_entries, NULL);

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

    /* Get URI from remaining args */
    if (argc < 2)
    {
        g_printerr ("Error: Resource URI required\n");
        g_printerr ("Usage: mcp-read [OPTIONS] -- URI\n");
        return MCP_CLI_EXIT_ERROR;
    }

    uri = argv[1];

    /* Create transport */
    transport = mcp_cli_create_transport (&error);
    if (transport == NULL)
    {
        g_printerr ("Error: %s\n", error->message);
        return MCP_CLI_EXIT_ERROR;
    }

    /* Create and connect client */
    client = mcp_client_new ("mcp-read", "1.0.0");
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

    /* Initialize read context */
    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);

    /* Read the resource */
    if (!mcp_cli_opt_quiet && !mcp_cli_opt_json)
    {
        g_print ("Reading '%s'...\n", uri);
    }

    mcp_client_read_resource_async (client, uri, NULL, on_read_resource_complete, &ctx);

    /* Wait for completion */
    g_main_loop_run (ctx.loop);
    g_main_loop_unref (ctx.loop);

    /* Handle result */
    if (ctx.error != NULL)
    {
        if (g_error_matches (ctx.error, MCP_ERROR, MCP_ERROR_RESOURCE_NOT_FOUND))
        {
            g_printerr ("Error: Resource '%s' not found\n", uri);
            exit_code = MCP_CLI_EXIT_NOT_FOUND;
        }
        else
        {
            g_printerr ("Error reading resource: %s\n", ctx.error->message);
            exit_code = MCP_CLI_EXIT_ERROR;
        }
        g_error_free (ctx.error);
    }
    else if (ctx.contents != NULL)
    {
        /* Handle output to file if specified */
        if (opt_output != NULL)
        {
            GList *l;
            g_autoptr(GString) output = g_string_new (NULL);

            for (l = ctx.contents; l != NULL; l = l->next)
            {
                McpResourceContents *c = l->data;
                const gchar *text = mcp_resource_contents_get_text (c);
                const gchar *blob = mcp_resource_contents_get_blob (c);

                if (text != NULL)
                {
                    g_string_append (output, text);
                }
                else if (blob != NULL)
                {
                    gsize blob_len;
                    g_autofree guchar *decoded = NULL;

                    decoded = g_base64_decode (blob, &blob_len);
                    g_string_append_len (output, (const gchar *)decoded, blob_len);
                }
            }

            if (!g_file_set_contents (opt_output, output->str, output->len, &error))
            {
                g_printerr ("Error writing to '%s': %s\n", opt_output, error->message);
                exit_code = MCP_CLI_EXIT_ERROR;
            }
            else
            {
                if (!mcp_cli_opt_quiet)
                {
                    g_print ("Wrote %lu bytes to '%s'\n", (unsigned long)output->len, opt_output);
                }
                exit_code = MCP_CLI_EXIT_SUCCESS;
            }
        }
        else
        {
            /* Print to stdout */
            if (!mcp_cli_opt_quiet && !mcp_cli_opt_json)
            {
                g_print ("\n");
            }
            mcp_cli_print_resource_contents (ctx.contents, mcp_cli_opt_json);
            exit_code = MCP_CLI_EXIT_SUCCESS;
        }

        g_list_free_full (ctx.contents, (GDestroyNotify)mcp_resource_contents_unref);
    }
    else
    {
        g_printerr ("Error: No content received\n");
        exit_code = MCP_CLI_EXIT_ERROR;
    }

    /* Disconnect */
    mcp_cli_disconnect_sync (client, NULL);

    return exit_code;
}
