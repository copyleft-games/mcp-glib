/*
 * websocket-server.c - MCP Server over WebSocket Example
 *
 * This example demonstrates how to create an MCP server that listens
 * for connections over WebSocket for bidirectional communication.
 *
 * To run:
 *   ./websocket-server [--port PORT] [--host HOST] [--path PATH]
 *
 * Then connect with an MCP client using WebSocket transport:
 *   ws://localhost:8081/
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "mcp.h"
#include <stdlib.h>

static GMainLoop *main_loop = NULL;
static guint      port = 8081;
static gchar     *host = NULL;
static gchar     *path = NULL;
static gboolean   require_auth = FALSE;
static gchar     *auth_token = NULL;
static guint      keepalive = 30;

/* ========================================================================== */
/* Helper Functions                                                           */
/* ========================================================================== */

static const gchar *
state_to_string (McpTransportState state)
{
    switch (state)
    {
    case MCP_TRANSPORT_STATE_DISCONNECTED:
        return "disconnected";
    case MCP_TRANSPORT_STATE_CONNECTING:
        return "connecting";
    case MCP_TRANSPORT_STATE_CONNECTED:
        return "connected";
    case MCP_TRANSPORT_STATE_DISCONNECTING:
        return "disconnecting";
    case MCP_TRANSPORT_STATE_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

/* ========================================================================== */
/* Tool Handler                                                               */
/* ========================================================================== */

/*
 * echo_handler:
 * @server: the MCP server
 * @name: tool name ("echo")
 * @arguments: JSON object with tool arguments
 * @user_data: unused
 *
 * Handles the "echo" tool call. Returns the "message" argument back.
 *
 * Returns: (transfer full): the tool result
 */
static McpToolResult *
echo_handler (McpServer   *server,
              const gchar *name,
              JsonObject  *arguments,
              gpointer     user_data)
{
    McpToolResult *result;
    const gchar *message = NULL;

    /* Get the message argument */
    if (arguments != NULL && json_object_has_member (arguments, "message"))
    {
        message = json_object_get_string_member (arguments, "message");
    }

    if (message == NULL)
    {
        message = "(no message provided)";
    }

    /* Create success result with the echoed message */
    result = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_text (result, message);

    return result;
}

/*
 * server_info_handler:
 * @server: the MCP server
 * @name: tool name ("server_info")
 * @arguments: JSON object with tool arguments (unused)
 * @user_data: the WebSocket transport
 *
 * Returns information about the WebSocket server.
 *
 * Returns: (transfer full): the tool result
 */
static McpToolResult *
server_info_handler (McpServer   *server,
                     const gchar *name,
                     JsonObject  *arguments,
                     gpointer     user_data)
{
    McpWebSocketServerTransport *transport = MCP_WEBSOCKET_SERVER_TRANSPORT (user_data);
    McpToolResult *result;
    g_autofree gchar *info = NULL;
    guint actual_port;

    actual_port = mcp_websocket_server_transport_get_actual_port (transport);

    info = g_strdup_printf (
        "WebSocket Server Information:\n"
        "  Port: %u\n"
        "  Path: %s\n"
        "  Client Connected: %s\n"
        "  Keepalive Interval: %u seconds",
        actual_port,
        mcp_websocket_server_transport_get_path (transport),
        mcp_websocket_server_transport_has_client (transport) ? "yes" : "no",
        mcp_websocket_server_transport_get_keepalive_interval (transport));

    result = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_text (result, info);

    return result;
}

/* ========================================================================== */
/* Resource Handler                                                           */
/* ========================================================================== */

/*
 * readme_handler:
 * @server: the MCP server
 * @uri: resource URI ("file:///readme")
 * @user_data: unused
 *
 * Returns the README content as a resource.
 *
 * Returns: (transfer full) (element-type McpResourceContents): list of contents
 */
static GList *
readme_handler (McpServer   *server,
                const gchar *uri,
                gpointer     user_data)
{
    McpResourceContents *contents;
    const gchar *readme_text =
        "# WebSocket MCP Server\n\n"
        "This MCP server communicates over WebSocket.\n\n"
        "## Transport Details\n\n"
        "- **Bidirectional**: Full-duplex JSON-RPC over WebSocket text frames\n"
        "- **Keepalive**: Periodic ping frames to maintain connection\n\n"
        "## Available Tools\n\n"
        "- **echo**: Echoes back your message\n"
        "- **server_info**: Returns WebSocket server information\n\n"
        "## Authentication\n\n"
        "When `--token` is specified, clients must include:\n"
        "```\n"
        "Authorization: Bearer <token>\n"
        "```\n"
        "in the WebSocket handshake headers.\n";

    contents = mcp_resource_contents_new_text (uri, readme_text, "text/markdown");

    return g_list_append (NULL, contents);
}

/* ========================================================================== */
/* Signal Handlers                                                            */
/* ========================================================================== */

static void
on_client_initialized (McpServer *server,
                       gpointer   user_data)
{
    g_message ("Client connected and initialized via WebSocket");
}

static void
on_client_disconnected (McpServer *server,
                        gpointer   user_data)
{
    g_message ("Client disconnected");
}

static void
on_transport_state_changed (McpTransport      *transport,
                            McpTransportState  old_state,
                            McpTransportState  new_state,
                            gpointer           user_data)
{
    g_message ("Transport state: %s -> %s",
               state_to_string (old_state),
               state_to_string (new_state));
}

static void
on_server_started (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    McpServer *server = MCP_SERVER (source);
    McpWebSocketServerTransport *transport = MCP_WEBSOCKET_SERVER_TRANSPORT (user_data);
    g_autoptr(GError) error = NULL;
    guint actual_port;

    if (!mcp_server_start_finish (server, result, &error))
    {
        g_printerr ("Failed to start server: %s\n", error->message);
        g_main_loop_quit (main_loop);
        return;
    }

    actual_port = mcp_websocket_server_transport_get_actual_port (transport);
    g_message ("WebSocket Server listening on port %u", actual_port);
    g_message ("  WebSocket URL: ws://%s:%u%s",
               host ? host : "0.0.0.0",
               actual_port,
               mcp_websocket_server_transport_get_path (transport));

    if (require_auth)
    {
        g_message ("  Authentication: required (Bearer token)");
    }

    g_message ("  Keepalive: %u seconds",
               mcp_websocket_server_transport_get_keepalive_interval (transport));
}

/* ========================================================================== */
/* Command Line Options                                                       */
/* ========================================================================== */

static GOptionEntry entries[] = {
    { "port", 'p', 0, G_OPTION_ARG_INT, &port,
      "Port to listen on (default: 8081, 0 for auto)", "PORT" },
    { "host", 'H', 0, G_OPTION_ARG_STRING, &host,
      "Host/address to bind to (default: all interfaces)", "HOST" },
    { "path", 'P', 0, G_OPTION_ARG_STRING, &path,
      "WebSocket endpoint path (default: /)", "PATH" },
    { "token", 't', 0, G_OPTION_ARG_STRING, &auth_token,
      "Require Bearer token authentication", "TOKEN" },
    { "keepalive", 'k', 0, G_OPTION_ARG_INT, &keepalive,
      "Keepalive ping interval in seconds (default: 30, 0 to disable)", "SECONDS" },
    { NULL }
};

/* ========================================================================== */
/* Main Entry Point                                                           */
/* ========================================================================== */

int
main (int   argc,
      char *argv[])
{
    g_autoptr(GOptionContext) context = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpWebSocketServerTransport) transport = NULL;
    g_autoptr(McpTool) echo_tool = NULL;
    g_autoptr(McpTool) info_tool = NULL;
    g_autoptr(McpResource) readme_resource = NULL;

    /* Parse command line options */
    context = g_option_context_new ("- MCP WebSocket Server Example");
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_printerr ("Option parsing failed: %s\n", error->message);
        return 1;
    }

    /* Check if auth token implies require_auth */
    if (auth_token != NULL && auth_token[0] != '\0')
    {
        require_auth = TRUE;
    }

    /* Create the main loop */
    main_loop = g_main_loop_new (NULL, FALSE);

    /* Create the WebSocket transport */
    transport = mcp_websocket_server_transport_new_full (host, port, path ? path : "/");

    /* Configure authentication if requested */
    if (require_auth)
    {
        mcp_websocket_server_transport_set_require_auth (transport, TRUE);
        mcp_websocket_server_transport_set_auth_token (transport, auth_token);
    }

    /* Configure keepalive */
    mcp_websocket_server_transport_set_keepalive_interval (transport, keepalive);

    /* Connect transport signals */
    g_signal_connect (transport, "state-changed",
                      G_CALLBACK (on_transport_state_changed), NULL);

    /* Create the server */
    server = mcp_server_new ("websocket-server", "1.0.0");
    mcp_server_set_instructions (server,
        "This is an MCP server accessible over WebSocket. "
        "Use the 'echo' tool to echo messages, "
        "the 'server_info' tool to get server details, "
        "and read 'file:///readme' for documentation.");

    /* Connect server signals */
    g_signal_connect (server, "client-initialized",
                      G_CALLBACK (on_client_initialized), transport);
    g_signal_connect (server, "client-disconnected",
                      G_CALLBACK (on_client_disconnected), NULL);

    /* Create and add the echo tool */
    echo_tool = mcp_tool_new ("echo", "Echoes back the provided message");
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonNode) schema = NULL;

        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "type");
        json_builder_add_string_value (builder, "object");
        json_builder_set_member_name (builder, "properties");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "message");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "type");
        json_builder_add_string_value (builder, "string");
        json_builder_set_member_name (builder, "description");
        json_builder_add_string_value (builder, "The message to echo back");
        json_builder_end_object (builder);
        json_builder_end_object (builder);
        json_builder_set_member_name (builder, "required");
        json_builder_begin_array (builder);
        json_builder_add_string_value (builder, "message");
        json_builder_end_array (builder);
        json_builder_end_object (builder);

        schema = json_builder_get_root (builder);
        mcp_tool_set_input_schema (echo_tool, schema);
    }
    mcp_server_add_tool (server, echo_tool, echo_handler, NULL, NULL);

    /* Create and add the server_info tool */
    info_tool = mcp_tool_new ("server_info", "Returns WebSocket server information");
    mcp_server_add_tool (server, info_tool, server_info_handler, transport, NULL);

    /* Create and add the readme resource */
    readme_resource = mcp_resource_new ("file:///readme", "README");
    mcp_resource_set_description (readme_resource, "Server documentation");
    mcp_resource_set_mime_type (readme_resource, "text/markdown");
    mcp_server_add_resource (server, readme_resource, readme_handler, NULL, NULL);

    /* Attach transport to server */
    mcp_server_set_transport (server, MCP_TRANSPORT (transport));

    /* Start the server */
    g_message ("Starting WebSocket MCP Server...");
    mcp_server_start_async (server, NULL, on_server_started, transport);

    /* Run the main loop */
    g_main_loop_run (main_loop);

    /* Cleanup */
    g_main_loop_unref (main_loop);
    main_loop = NULL;
    g_free (host);
    g_free (path);
    g_free (auth_token);

    g_message ("Server shut down");

    return 0;
}
