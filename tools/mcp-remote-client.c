/*
 * mcp-remote-client.c - MCP Remote Client Proxy
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Acts as a stdio MCP server that proxies all messages to a remote
 * MCP server via HTTP or WebSocket. This allows tools that only support
 * stdio MCP servers to connect to remote HTTP/WS MCP servers.
 *
 * Usage:
 *   mcp-remote-client --http https://api.example.com/mcp
 *   mcp-remote-client --ws wss://api.example.com/mcp
 *   mcp-remote-client --http https://api.example.com/mcp --token "Bearer xxx"
 *
 * Exit codes:
 *   0 - Clean shutdown
 *   1 - Connection/transport error
 */

#include <mcp.h>
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <string.h>

/* ========================================================================== */
/* License text                                                                */
/* ========================================================================== */

#define MCP_CLI_LICENSE \
    "mcp-glib CLI Tools\n" \
    "Copyright (C) 2025 Copyleft Games\n" \
    "\n" \
    "This program is free software: you can redistribute it and/or modify\n" \
    "it under the terms of the GNU Affero General Public License as published\n" \
    "by the Free Software Foundation, either version 3 of the License, or\n" \
    "(at your option) any later version.\n" \
    "\n" \
    "This program is distributed in the hope that it will be useful,\n" \
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n" \
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" \
    "GNU Affero General Public License for more details.\n" \
    "\n" \
    "You should have received a copy of the GNU Affero General Public License\n" \
    "along with this program.  If not, see <https://www.gnu.org/licenses/>.\n"

#define MCP_CLI_BUG_URL "https://gitlab.com/copyleftgames/mcp-glib/-/issues"

/* ========================================================================== */
/* Exit codes                                                                  */
/* ========================================================================== */

#define EXIT_SUCCESS_CODE   (0)
#define EXIT_ERROR_CODE     (1)

/* ========================================================================== */
/* Command-line options                                                        */
/* ========================================================================== */

static gchar    *opt_http = NULL;
static gchar    *opt_ws = NULL;
static gchar    *opt_token = NULL;
static gboolean  opt_license = FALSE;
static gboolean  opt_quiet = FALSE;

static GOptionEntry entries[] =
{
    {
        "http", 'h', 0, G_OPTION_ARG_STRING, &opt_http,
        "Connect to HTTP MCP server at URL", "URL"
    },
    {
        "ws", 'w', 0, G_OPTION_ARG_STRING, &opt_ws,
        "Connect to WebSocket MCP server at URL", "URL"
    },
    {
        "token", 't', 0, G_OPTION_ARG_STRING, &opt_token,
        "Authorization token for remote server", "TOKEN"
    },
    {
        "quiet", 'q', 0, G_OPTION_ARG_NONE, &opt_quiet,
        "Suppress status messages on stderr", NULL
    },
    {
        "license", 0, 0, G_OPTION_ARG_NONE, &opt_license,
        "Show license information", NULL
    },
    { NULL }
};

/* ========================================================================== */
/* Proxy context                                                               */
/* ========================================================================== */

typedef struct
{
    GMainLoop      *loop;
    McpTransport   *stdio_transport;
    McpTransport   *remote_transport;
    gboolean        remote_connected;
    gboolean        shutting_down;
    gint            exit_code;
} ProxyContext;

/* ========================================================================== */
/* Forward message helper                                                      */
/* ========================================================================== */

typedef struct
{
    ProxyContext *ctx;
    McpTransport *target;
    gchar        *direction;  /* "local->remote" or "remote->local" */
} ForwardData;

static void
on_forward_complete (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
    ForwardData *data = user_data;
    g_autoptr(GError) error = NULL;

    if (!mcp_transport_send_message_finish (MCP_TRANSPORT (source), result, &error))
    {
        if (!data->ctx->shutting_down)
        {
            g_printerr ("Failed to forward message (%s): %s\n",
                        data->direction, error->message);
        }
    }

    g_free (data->direction);
    g_free (data);
}

static void
forward_message (ProxyContext *ctx,
                 McpTransport *target,
                 JsonNode     *message,
                 const gchar  *direction)
{
    ForwardData *data;

    data = g_new0 (ForwardData, 1);
    data->ctx = ctx;
    data->target = target;
    data->direction = g_strdup (direction);

    mcp_transport_send_message_async (target, message, NULL,
                                      on_forward_complete, data);
}

/* ========================================================================== */
/* Transport signal handlers                                                   */
/* ========================================================================== */

static void
on_stdio_message_received (McpTransport *transport,
                           JsonNode     *message,
                           ProxyContext *ctx)
{
    /* Forward messages from local (stdio) to remote */
    if (ctx->remote_connected && !ctx->shutting_down)
    {
        forward_message (ctx, ctx->remote_transport, message, "local->remote");
    }
}

static void
on_remote_message_received (McpTransport *transport,
                            JsonNode     *message,
                            ProxyContext *ctx)
{
    /* Forward messages from remote to local (stdio) */
    if (!ctx->shutting_down)
    {
        forward_message (ctx, ctx->stdio_transport, message, "remote->local");
    }
}

static void
on_stdio_state_changed (McpTransport      *transport,
                        McpTransportState  old_state,
                        McpTransportState  new_state,
                        ProxyContext      *ctx)
{
    if (new_state == MCP_TRANSPORT_STATE_DISCONNECTED && !ctx->shutting_down)
    {
        if (!opt_quiet)
        {
            g_printerr ("Local transport disconnected, shutting down\n");
        }
        ctx->shutting_down = TRUE;
        g_main_loop_quit (ctx->loop);
    }
}

static void
on_remote_state_changed (McpTransport      *transport,
                         McpTransportState  old_state,
                         McpTransportState  new_state,
                         ProxyContext      *ctx)
{
    if (new_state == MCP_TRANSPORT_STATE_CONNECTED)
    {
        ctx->remote_connected = TRUE;
        if (!opt_quiet)
        {
            g_printerr ("Connected to remote server\n");
        }
    }
    else if (new_state == MCP_TRANSPORT_STATE_DISCONNECTED && !ctx->shutting_down)
    {
        if (!opt_quiet)
        {
            g_printerr ("Remote transport disconnected, shutting down\n");
        }
        ctx->shutting_down = TRUE;
        ctx->exit_code = EXIT_ERROR_CODE;
        g_main_loop_quit (ctx->loop);
    }
}

static void
on_remote_error (McpTransport *transport,
                 GError       *error,
                 ProxyContext *ctx)
{
    if (!ctx->shutting_down)
    {
        g_printerr ("Remote transport error: %s\n", error->message);
    }
}

static void
on_stdio_error (McpTransport *transport,
                GError       *error,
                ProxyContext *ctx)
{
    if (!ctx->shutting_down)
    {
        g_printerr ("Local transport error: %s\n", error->message);
    }
}

/* ========================================================================== */
/* Connection callbacks                                                        */
/* ========================================================================== */

static void
on_remote_connect_complete (GObject      *source,
                            GAsyncResult *result,
                            gpointer      user_data)
{
    ProxyContext *ctx = user_data;
    g_autoptr(GError) error = NULL;

    if (!mcp_transport_connect_finish (MCP_TRANSPORT (source), result, &error))
    {
        g_printerr ("Failed to connect to remote server: %s\n", error->message);
        ctx->exit_code = EXIT_ERROR_CODE;
        g_main_loop_quit (ctx->loop);
        return;
    }

    /* Remote is connected, now start the stdio transport */
    if (!opt_quiet)
    {
        g_printerr ("Remote connected, starting local stdio transport\n");
    }

    mcp_transport_connect_async (ctx->stdio_transport, NULL, NULL, NULL);
}

/* ========================================================================== */
/* Create transports                                                           */
/* ========================================================================== */

static McpTransport *
create_remote_transport (GError **error)
{
    McpTransport *transport = NULL;

    if (opt_http != NULL)
    {
        transport = MCP_TRANSPORT (mcp_http_transport_new (opt_http));
        if (opt_token != NULL)
        {
            mcp_http_transport_set_auth_token (MCP_HTTP_TRANSPORT (transport),
                                               opt_token);
        }
    }
    else if (opt_ws != NULL)
    {
        transport = MCP_TRANSPORT (mcp_websocket_transport_new (opt_ws));
        if (opt_token != NULL)
        {
            mcp_websocket_transport_set_auth_token (
                MCP_WEBSOCKET_TRANSPORT (transport), opt_token);
        }
    }
    else
    {
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_INVALID_ARGUMENT,
                     "Either --http or --ws must be specified");
        return NULL;
    }

    return transport;
}

/* ========================================================================== */
/* Main entry point                                                            */
/* ========================================================================== */

static const gchar *description =
    "Acts as a stdio MCP server that proxies all messages to a remote\n"
    "MCP server via HTTP or WebSocket. This allows tools that only support\n"
    "stdio MCP servers to connect to remote HTTP/WS MCP servers.\n"
    "\n"
    "Examples:\n"
    "  mcp-remote-client --http https://api.example.com/mcp\n"
    "  mcp-remote-client --ws wss://api.example.com/mcp\n"
    "  mcp-remote-client -h https://api.example.com/mcp -t \"Bearer token\"\n"
    "\n"
    "Exit codes:\n"
    "  0  Clean shutdown\n"
    "  1  Connection/transport error\n"
    "\n"
    "Report bugs to: " MCP_CLI_BUG_URL;

int
main (int    argc,
      char **argv)
{
    g_autoptr(GOptionContext) context = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(McpTransport) stdio_transport = NULL;
    g_autoptr(McpTransport) remote_transport = NULL;
    ProxyContext ctx;

    /* Parse command line options */
    context = g_option_context_new (NULL);
    g_option_context_set_summary (context,
        "Proxy stdio MCP connections to a remote HTTP/WebSocket MCP server");
    g_option_context_set_description (context, description);
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_printerr ("Option parsing failed: %s\n", error->message);
        return EXIT_ERROR_CODE;
    }

    /* Handle --license */
    if (opt_license)
    {
        g_print ("%s", MCP_CLI_LICENSE);
        return EXIT_SUCCESS_CODE;
    }

    /* Validate options */
    if (opt_http == NULL && opt_ws == NULL)
    {
        g_printerr ("Error: Either --http or --ws must be specified\n");
        g_printerr ("Use --help for usage information\n");
        return EXIT_ERROR_CODE;
    }

    if (opt_http != NULL && opt_ws != NULL)
    {
        g_printerr ("Error: Cannot specify both --http and --ws\n");
        return EXIT_ERROR_CODE;
    }

    /* Create remote transport */
    remote_transport = create_remote_transport (&error);
    if (remote_transport == NULL)
    {
        g_printerr ("Error: %s\n", error->message);
        return EXIT_ERROR_CODE;
    }

    /* Create stdio transport for local side */
    stdio_transport = MCP_TRANSPORT (mcp_stdio_transport_new ());

    /* Initialize context */
    memset (&ctx, 0, sizeof (ctx));
    ctx.loop = g_main_loop_new (NULL, FALSE);
    ctx.stdio_transport = stdio_transport;
    ctx.remote_transport = remote_transport;
    ctx.exit_code = EXIT_SUCCESS_CODE;

    /* Connect signal handlers */
    g_signal_connect (stdio_transport, "message-received",
                      G_CALLBACK (on_stdio_message_received), &ctx);
    g_signal_connect (stdio_transport, "state-changed",
                      G_CALLBACK (on_stdio_state_changed), &ctx);
    g_signal_connect (stdio_transport, "error",
                      G_CALLBACK (on_stdio_error), &ctx);

    g_signal_connect (remote_transport, "message-received",
                      G_CALLBACK (on_remote_message_received), &ctx);
    g_signal_connect (remote_transport, "state-changed",
                      G_CALLBACK (on_remote_state_changed), &ctx);
    g_signal_connect (remote_transport, "error",
                      G_CALLBACK (on_remote_error), &ctx);

    /* Start connection to remote server first */
    if (!opt_quiet)
    {
        g_printerr ("Connecting to remote server...\n");
    }

    mcp_transport_connect_async (remote_transport, NULL,
                                 on_remote_connect_complete, &ctx);

    /* Run main loop */
    g_main_loop_run (ctx.loop);

    /* Cleanup */
    ctx.shutting_down = TRUE;

    if (mcp_transport_is_connected (remote_transport))
    {
        mcp_transport_disconnect_async (remote_transport, NULL, NULL, NULL);
    }

    if (mcp_transport_is_connected (stdio_transport))
    {
        mcp_transport_disconnect_async (stdio_transport, NULL, NULL, NULL);
    }

    g_main_loop_unref (ctx.loop);

    return ctx.exit_code;
}
