/*
 * simple-server.c - Minimal MCP Server Example
 *
 * This example demonstrates how to create a basic MCP server with:
 *   - A simple "echo" tool that returns its input
 *   - A static resource
 *   - A greeting prompt
 *
 * To run:
 *   ./simple-server
 *
 * The server communicates via stdin/stdout using JSON-RPC.
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <mcp/mcp.h>

static GMainLoop *main_loop = NULL;

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
echo_handler (McpServer  *server,
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
        "# Simple MCP Server\n\n"
        "This is a minimal MCP server example.\n\n"
        "## Available Tools\n\n"
        "- **echo**: Echoes back your message\n\n"
        "## Available Prompts\n\n"
        "- **greeting**: Returns a friendly greeting\n";

    contents = mcp_resource_contents_new_text (uri, readme_text, "text/markdown");

    return g_list_append (NULL, contents);
}

/* ========================================================================== */
/* Prompt Handler                                                             */
/* ========================================================================== */

/*
 * greeting_handler:
 * @server: the MCP server
 * @name: prompt name ("greeting")
 * @arguments: hash table of arguments (contains "name")
 * @user_data: unused
 *
 * Returns a greeting message based on the provided name.
 *
 * Returns: (transfer full): the prompt result
 */
static McpPromptResult *
greeting_handler (McpServer   *server,
                  const gchar *name,
                  GHashTable  *arguments,
                  gpointer     user_data)
{
    McpPromptResult *result;
    McpPromptMessage *message;
    const gchar *user_name = NULL;
    g_autofree gchar *greeting = NULL;

    /* Get the name argument */
    if (arguments != NULL)
    {
        user_name = g_hash_table_lookup (arguments, "name");
    }

    if (user_name == NULL || user_name[0] == '\0')
    {
        user_name = "friend";
    }

    /* Create the greeting message */
    greeting = g_strdup_printf ("Hello, %s! Welcome to the Simple MCP Server. "
                                "How can I help you today?", user_name);

    /* Build the result */
    result = mcp_prompt_result_new ("A friendly greeting");
    message = mcp_prompt_message_new (MCP_ROLE_ASSISTANT);
    mcp_prompt_message_add_text (message, greeting);
    mcp_prompt_result_add_message (result, message);
    mcp_prompt_message_unref (message);

    return result;
}

/* ========================================================================== */
/* Signal Handlers                                                            */
/* ========================================================================== */

static void
on_client_initialized (McpServer *server,
                       gpointer   user_data)
{
    g_message ("Client connected and initialized");
}

static void
on_client_disconnected (McpServer *server,
                        gpointer   user_data)
{
    g_message ("Client disconnected, shutting down");
    g_main_loop_quit (main_loop);
}

/* ========================================================================== */
/* Main Entry Point                                                           */
/* ========================================================================== */

int
main (int   argc,
      char *argv[])
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpStdioTransport) transport = NULL;
    g_autoptr(McpTool) echo_tool = NULL;
    g_autoptr(McpResource) readme_resource = NULL;
    g_autoptr(McpPrompt) greeting_prompt = NULL;

    /* Create the main loop */
    main_loop = g_main_loop_new (NULL, FALSE);

    /* Create the server */
    server = mcp_server_new ("simple-server", "1.0.0");
    mcp_server_set_instructions (server,
        "This is a simple MCP server example. "
        "Use the 'echo' tool to echo messages, "
        "read the 'file:///readme' resource for documentation, "
        "and use the 'greeting' prompt for a friendly hello.");

    /* Connect signals */
    g_signal_connect (server, "client-initialized",
                      G_CALLBACK (on_client_initialized), NULL);
    g_signal_connect (server, "client-disconnected",
                      G_CALLBACK (on_client_disconnected), NULL);

    /* Create and add the echo tool */
    echo_tool = mcp_tool_new ("echo", "Echoes back the provided message");
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonNode) schema = NULL;

        /* Define the input schema */
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

    /* Create and add the readme resource */
    readme_resource = mcp_resource_new ("file:///readme", "README");
    mcp_resource_set_description (readme_resource, "Server documentation");
    mcp_resource_set_mime_type (readme_resource, "text/markdown");
    mcp_server_add_resource (server, readme_resource, readme_handler, NULL, NULL);

    /* Create and add the greeting prompt */
    greeting_prompt = mcp_prompt_new ("greeting", "Returns a friendly greeting");
    mcp_prompt_add_argument_full (greeting_prompt, "name", "The name to greet", FALSE);
    mcp_server_add_prompt (server, greeting_prompt, greeting_handler, NULL, NULL);

    /* Create stdio transport and attach to server */
    transport = mcp_stdio_transport_new ();
    mcp_server_set_transport (server, MCP_TRANSPORT (transport));

    /* Start the server */
    g_message ("Starting Simple MCP Server...");
    mcp_server_start_async (server, NULL, NULL, NULL);

    /* Run the main loop */
    g_main_loop_run (main_loop);

    /* Cleanup */
    g_main_loop_unref (main_loop);
    main_loop = NULL;

    g_message ("Server shut down");

    return 0;
}
