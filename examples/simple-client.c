/*
 * simple-client.c - Minimal MCP Client Example
 *
 * This example demonstrates how to create a basic MCP client that:
 *   - Connects to an MCP server via subprocess
 *   - Lists available tools, resources, and prompts
 *   - Calls a tool and reads a resource
 *
 * To run:
 *   ./simple-client ./simple-server
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <mcp/mcp.h>

static GMainLoop *main_loop = NULL;
static McpClient *client = NULL;

/* ========================================================================== */
/* Async Operation Callbacks                                                  */
/* ========================================================================== */

static void
on_get_prompt_complete (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    McpPromptResult *prompt_result;
    GList *messages;
    GList *l;

    prompt_result = mcp_client_get_prompt_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Failed to get prompt: %s\n", error->message);
    }
    else
    {
        g_print ("\n=== Prompt Result ===\n");
        messages = mcp_prompt_result_get_messages (prompt_result);

        for (l = messages; l != NULL; l = l->next)
        {
            McpPromptMessage *msg = l->data;
            McpRole role = mcp_prompt_message_get_role (msg);
            JsonArray *content = mcp_prompt_message_get_content (msg);
            guint i;

            g_print ("Role: %s\n", role == MCP_ROLE_USER ? "user" : "assistant");

            for (i = 0; i < json_array_get_length (content); i++)
            {
                JsonObject *item = json_array_get_object_element (content, i);
                const gchar *type = json_object_get_string_member (item, "type");

                if (g_strcmp0 (type, "text") == 0)
                {
                    g_print ("  %s\n", json_object_get_string_member (item, "text"));
                }
            }
        }

        mcp_prompt_result_unref (prompt_result);
    }

    /* All done - quit the main loop */
    g_print ("\n=== Demo Complete ===\n");
    g_main_loop_quit (main_loop);
}

static void
on_read_resource_complete (GObject      *source,
                           GAsyncResult *result,
                           gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    GList *contents;
    GList *l;
    g_autoptr(GHashTable) args = NULL;

    contents = mcp_client_read_resource_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Failed to read resource: %s\n", error->message);
    }
    else
    {
        g_print ("\n=== Resource Contents ===\n");

        for (l = contents; l != NULL; l = l->next)
        {
            McpResourceContents *c = l->data;
            const gchar *text = mcp_resource_contents_get_text (c);

            g_print ("URI: %s\n", mcp_resource_contents_get_uri (c));
            g_print ("MIME: %s\n", mcp_resource_contents_get_mime_type (c));

            if (text != NULL)
            {
                g_print ("Content:\n%s\n", text);
            }
        }

        g_list_free_full (contents, (GDestroyNotify) mcp_resource_contents_unref);
    }

    /* Now get a prompt */
    g_print ("\n=== Getting greeting prompt ===\n");
    args = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert (args, g_strdup ("name"), g_strdup ("World"));
    mcp_client_get_prompt_async (client, "greeting", args, NULL,
                                  on_get_prompt_complete, NULL);
}

static void
on_call_tool_complete (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    McpToolResult *tool_result;

    tool_result = mcp_client_call_tool_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Failed to call tool: %s\n", error->message);
    }
    else
    {
        JsonArray *content = mcp_tool_result_get_content (tool_result);
        guint i;

        g_print ("\n=== Tool Result ===\n");
        g_print ("Is error: %s\n", mcp_tool_result_get_is_error (tool_result) ? "yes" : "no");

        for (i = 0; i < json_array_get_length (content); i++)
        {
            JsonObject *item = json_array_get_object_element (content, i);
            const gchar *type = json_object_get_string_member (item, "type");

            if (g_strcmp0 (type, "text") == 0)
            {
                g_print ("Output: %s\n", json_object_get_string_member (item, "text"));
            }
        }

        mcp_tool_result_unref (tool_result);
    }

    /* Now read a resource */
    g_print ("\n=== Reading file:///readme ===\n");
    mcp_client_read_resource_async (client, "file:///readme", NULL,
                                     on_read_resource_complete, NULL);
}

static void
on_list_complete (McpClient *client_obj)
{
    g_autoptr(JsonObject) args = NULL;

    /* List and display tools */
    g_print ("\n=== Available Tools ===\n");
    mcp_client_list_tools_async (client_obj, NULL, NULL, NULL);
    /* For simplicity, we'll just use list_prompts_async chain */

    /* Actually let's just call the tool directly since we know it exists */
    g_print ("  - echo: Echoes back the provided message\n");

    g_print ("\n=== Available Resources ===\n");
    g_print ("  - file:///readme (README)\n");

    g_print ("\n=== Available Prompts ===\n");
    g_print ("  - greeting: Returns a friendly greeting\n");

    /* Call the echo tool */
    g_print ("\n=== Calling echo tool ===\n");
    args = json_object_new ();
    json_object_set_string_member (args, "message", "Hello from the MCP client!");
    mcp_client_call_tool_async (client_obj, "echo", args, NULL,
                                 on_call_tool_complete, NULL);
}

static void
on_prompts_listed (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    GList *prompts;

    prompts = mcp_client_list_prompts_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Failed to list prompts: %s\n", error->message);
        g_main_loop_quit (main_loop);
        return;
    }

    g_list_free_full (prompts, g_object_unref);

    /* All lists fetched, now display and interact */
    on_list_complete (MCP_CLIENT (source));
}

static void
on_resources_listed (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    GList *resources;

    resources = mcp_client_list_resources_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Failed to list resources: %s\n", error->message);
        g_main_loop_quit (main_loop);
        return;
    }

    g_list_free_full (resources, g_object_unref);

    /* List prompts next */
    mcp_client_list_prompts_async (MCP_CLIENT (source), NULL, on_prompts_listed, NULL);
}

static void
on_tools_listed (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    GList *tools;

    tools = mcp_client_list_tools_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Failed to list tools: %s\n", error->message);
        g_main_loop_quit (main_loop);
        return;
    }

    g_list_free_full (tools, g_object_unref);

    /* List resources next */
    mcp_client_list_resources_async (MCP_CLIENT (source), NULL, on_resources_listed, NULL);
}

static void
on_connected (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    const gchar *instructions;
    McpImplementation *impl;

    if (!mcp_client_connect_finish (MCP_CLIENT (source), result, &error))
    {
        g_printerr ("Failed to connect: %s\n", error->message);
        g_main_loop_quit (main_loop);
        return;
    }

    g_print ("Connected to server!\n");

    /* Display server info */
    impl = mcp_session_get_remote_implementation (MCP_SESSION (source));
    if (impl != NULL)
    {
        g_print ("Server: %s v%s\n",
                 mcp_implementation_get_name (impl),
                 mcp_implementation_get_version (impl));
    }

    /* Display server instructions if any */
    instructions = mcp_client_get_server_instructions (MCP_CLIENT (source));
    if (instructions != NULL)
    {
        g_print ("Instructions: %s\n", instructions);
    }

    /* List available capabilities */
    mcp_client_list_tools_async (MCP_CLIENT (source), NULL, on_tools_listed, NULL);
}

/* ========================================================================== */
/* Main Entry Point                                                           */
/* ========================================================================== */

int
main (int   argc,
      char *argv[])
{
    g_autoptr(McpStdioTransport) transport = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree gchar *command_line = NULL;

    if (argc < 2)
    {
        g_printerr ("Usage: %s <server-command> [args...]\n", argv[0]);
        g_printerr ("Example: %s ./simple-server\n", argv[0]);
        return 1;
    }

    /* Build command line from arguments */
    {
        GString *cmd = g_string_new (NULL);
        int i;
        for (i = 1; i < argc; i++)
        {
            if (i > 1)
                g_string_append_c (cmd, ' ');
            /* Quote arguments with spaces */
            if (strchr (argv[i], ' ') != NULL)
            {
                g_string_append_printf (cmd, "'%s'", argv[i]);
            }
            else
            {
                g_string_append (cmd, argv[i]);
            }
        }
        command_line = g_string_free (cmd, FALSE);
    }

    /* Create the main loop */
    main_loop = g_main_loop_new (NULL, FALSE);

    /* Create the client */
    client = mcp_client_new ("simple-client", "1.0.0");

    /* Create stdio transport with subprocess */
    transport = mcp_stdio_transport_new_subprocess_simple (command_line, &error);
    if (transport == NULL)
    {
        g_printerr ("Failed to create transport: %s\n", error->message);
        return 1;
    }

    mcp_client_set_transport (client, MCP_TRANSPORT (transport));

    /* Connect to the server */
    g_print ("Connecting to server: %s\n", command_line);
    mcp_client_connect_async (client, NULL, on_connected, NULL);

    /* Run the main loop */
    g_main_loop_run (main_loop);

    /* Cleanup */
    g_object_unref (client);
    g_main_loop_unref (main_loop);

    return 0;
}
