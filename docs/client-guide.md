# MCP Client Guide

This guide explains how to create MCP clients using mcp-glib.

## Overview

An MCP client connects to servers to access their capabilities:
- **Tools** - Functions to call
- **Resources** - Data to read
- **Prompts** - Templates to use

## Basic Client Structure

```c
#include <mcp/mcp.h>

static GMainLoop *main_loop = NULL;
static McpClient *client = NULL;

static void
on_connected (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
    g_autoptr(GError) error = NULL;

    if (!mcp_client_connect_finish (MCP_CLIENT (source), result, &error))
    {
        g_printerr ("Connection failed: %s\n", error->message);
        g_main_loop_quit (main_loop);
        return;
    }

    g_print ("Connected to server!\n");

    /* Now you can list tools, resources, prompts and call them */
}

int
main (int argc, char *argv[])
{
    g_autoptr(McpStdioTransport) transport = NULL;
    g_autoptr(GError) error = NULL;

    main_loop = g_main_loop_new (NULL, FALSE);

    /* Create client with name and version */
    client = mcp_client_new ("my-client", "1.0.0");

    /* Create transport - spawn server as subprocess */
    transport = mcp_stdio_transport_new_subprocess_simple ("./my-server", &error);
    if (transport == NULL)
    {
        g_printerr ("Failed to create transport: %s\n", error->message);
        return 1;
    }

    /* Set transport and connect */
    mcp_client_set_transport (client, MCP_TRANSPORT (transport));
    mcp_client_connect_async (client, NULL, on_connected, NULL);

    /* Run main loop */
    g_main_loop_run (main_loop);

    /* Cleanup */
    g_object_unref (client);
    g_main_loop_unref (main_loop);

    return 0;
}
```

## Connecting to Servers

### Subprocess Transport

Spawn a server as a subprocess:

```c
g_autoptr(McpStdioTransport) transport = NULL;
g_autoptr(GError) error = NULL;

/* Simple version - just command */
transport = mcp_stdio_transport_new_subprocess_simple ("./my-server", &error);

/* With arguments */
transport = mcp_stdio_transport_new_subprocess_simple (
    "./my-server --port 8080", &error);
```

### Getting Server Info

After connecting, get server information:

```c
static void
on_connected (GObject *source, GAsyncResult *result, gpointer user_data)
{
    McpImplementation *impl;
    const gchar *instructions;

    /* ... check for errors ... */

    /* Get server implementation info */
    impl = mcp_session_get_remote_implementation (MCP_SESSION (source));
    if (impl != NULL)
    {
        g_print ("Server: %s v%s\n",
                 mcp_implementation_get_name (impl),
                 mcp_implementation_get_version (impl));
    }

    /* Get server instructions */
    instructions = mcp_client_get_server_instructions (MCP_CLIENT (source));
    if (instructions != NULL)
    {
        g_print ("Instructions: %s\n", instructions);
    }
}
```

## Listing Capabilities

### List Tools

```c
static void
on_tools_listed (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    GList *tools;
    GList *l;

    tools = mcp_client_list_tools_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Failed to list tools: %s\n", error->message);
        return;
    }

    g_print ("Available tools:\n");
    for (l = tools; l != NULL; l = l->next)
    {
        McpTool *tool = l->data;
        g_print ("  - %s: %s\n",
                 mcp_tool_get_name (tool),
                 mcp_tool_get_description (tool));
    }

    g_list_free_full (tools, g_object_unref);
}

/* Call it */
mcp_client_list_tools_async (client, NULL, on_tools_listed, NULL);
```

### List Resources

```c
static void
on_resources_listed (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    GList *resources;
    GList *l;

    resources = mcp_client_list_resources_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Failed to list resources: %s\n", error->message);
        return;
    }

    g_print ("Available resources:\n");
    for (l = resources; l != NULL; l = l->next)
    {
        McpResource *resource = l->data;
        g_print ("  - %s (%s)\n",
                 mcp_resource_get_uri (resource),
                 mcp_resource_get_name (resource));
    }

    g_list_free_full (resources, g_object_unref);
}

mcp_client_list_resources_async (client, NULL, on_resources_listed, NULL);
```

### List Prompts

```c
static void
on_prompts_listed (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    GList *prompts;
    GList *l;

    prompts = mcp_client_list_prompts_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Failed to list prompts: %s\n", error->message);
        return;
    }

    g_print ("Available prompts:\n");
    for (l = prompts; l != NULL; l = l->next)
    {
        McpPrompt *prompt = l->data;
        g_print ("  - %s: %s\n",
                 mcp_prompt_get_name (prompt),
                 mcp_prompt_get_description (prompt));
    }

    g_list_free_full (prompts, g_object_unref);
}

mcp_client_list_prompts_async (client, NULL, on_prompts_listed, NULL);
```

## Calling Tools

```c
static void
on_tool_called (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    McpToolResult *tool_result;
    JsonArray *content;
    guint i;

    tool_result = mcp_client_call_tool_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Tool call failed: %s\n", error->message);
        return;
    }

    /* Check if it's an error result */
    if (mcp_tool_result_get_is_error (tool_result))
    {
        g_printerr ("Tool returned error\n");
    }

    /* Process content */
    content = mcp_tool_result_get_content (tool_result);
    for (i = 0; i < json_array_get_length (content); i++)
    {
        JsonObject *item = json_array_get_object_element (content, i);
        const gchar *type = json_object_get_string_member (item, "type");

        if (g_strcmp0 (type, "text") == 0)
        {
            g_print ("Text: %s\n", json_object_get_string_member (item, "text"));
        }
        else if (g_strcmp0 (type, "image") == 0)
        {
            g_print ("Image: %s (%s)\n",
                     json_object_get_string_member (item, "mimeType"),
                     "base64 data...");
        }
    }

    mcp_tool_result_unref (tool_result);
}

/* Create arguments */
g_autoptr(JsonObject) args = json_object_new ();
json_object_set_string_member (args, "message", "Hello!");
json_object_set_int_member (args, "count", 5);

/* Call tool */
mcp_client_call_tool_async (client, "my-tool", args, NULL,
                            on_tool_called, NULL);
```

## Reading Resources

```c
static void
on_resource_read (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    GList *contents;
    GList *l;

    contents = mcp_client_read_resource_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Failed to read resource: %s\n", error->message);
        return;
    }

    for (l = contents; l != NULL; l = l->next)
    {
        McpResourceContents *c = l->data;
        const gchar *text;
        const gchar *blob;

        g_print ("URI: %s\n", mcp_resource_contents_get_uri (c));
        g_print ("MIME: %s\n", mcp_resource_contents_get_mime_type (c));

        /* Check for text content */
        text = mcp_resource_contents_get_text (c);
        if (text != NULL)
        {
            g_print ("Content:\n%s\n", text);
        }

        /* Check for binary content */
        blob = mcp_resource_contents_get_blob (c);
        if (blob != NULL)
        {
            g_print ("Binary data: %lu bytes (base64)\n", strlen (blob));
        }
    }

    g_list_free_full (contents, (GDestroyNotify) mcp_resource_contents_unref);
}

/* Read resource by URI */
mcp_client_read_resource_async (client, "file:///readme", NULL,
                                 on_resource_read, NULL);
```

## Getting Prompts

```c
static void
on_prompt_get (GObject      *source,
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
        return;
    }

    g_print ("Description: %s\n", mcp_prompt_result_get_description (prompt_result));

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

/* Create arguments hash table */
g_autoptr(GHashTable) args = g_hash_table_new_full (
    g_str_hash, g_str_equal, g_free, g_free);
g_hash_table_insert (args, g_strdup ("name"), g_strdup ("World"));

/* Get prompt */
mcp_client_get_prompt_async (client, "greeting", args, NULL,
                              on_prompt_get, NULL);
```

## Handling Notifications

The client receives notifications from the server:

```c
/* Tools changed */
g_signal_connect (client, "tools-changed",
                  G_CALLBACK (on_tools_changed), NULL);

/* Resources changed */
g_signal_connect (client, "resources-changed",
                  G_CALLBACK (on_resources_changed), NULL);

/* Prompts changed */
g_signal_connect (client, "prompts-changed",
                  G_CALLBACK (on_prompts_changed), NULL);

/* Resource updated (for subscribed resources) */
g_signal_connect (client, "resource-updated",
                  G_CALLBACK (on_resource_updated), NULL);
```

## Resource Subscriptions

Subscribe to resource updates:

```c
/* Subscribe */
mcp_client_subscribe_resource (client, "file:///config.json");

/* Handle updates */
static void
on_resource_updated (McpClient   *client,
                     const gchar *uri,
                     gpointer     user_data)
{
    g_print ("Resource updated: %s\n", uri);
    /* Re-read the resource */
    mcp_client_read_resource_async (client, uri, NULL,
                                     on_resource_read, NULL);
}

/* Unsubscribe */
mcp_client_unsubscribe_resource (client, "file:///config.json");
```

## Client Capabilities

Configure client capabilities before connecting:

```c
McpClientCapabilities *caps = mcp_client_get_capabilities (client);

/* Enable experimental features */
mcp_client_capabilities_set_experimental (caps, "tasks", TRUE);

/* Enable sampling */
mcp_client_capabilities_set_sampling (caps, TRUE);
```

## Error Handling

All async operations can fail. Always check for errors:

```c
static void
on_complete (GObject      *source,
             GAsyncResult *result,
             gpointer      user_data)
{
    g_autoptr(GError) error = NULL;

    /* Use the appropriate _finish function */
    if (!mcp_client_xxx_finish (MCP_CLIENT (source), result, &error))
    {
        /* Check error domain and code */
        if (g_error_matches (error, MCP_ERROR, MCP_ERROR_TIMEOUT))
        {
            g_printerr ("Request timed out\n");
        }
        else if (g_error_matches (error, MCP_ERROR, MCP_ERROR_CONNECTION_CLOSED))
        {
            g_printerr ("Server disconnected\n");
        }
        else
        {
            g_printerr ("Error: %s\n", error->message);
        }
        return;
    }

    /* Success - process result */
}
```

## Complete Example

See `examples/simple-client.c` for a complete working example that:
- Connects to a server subprocess
- Lists tools, resources, and prompts
- Calls a tool
- Reads a resource
- Gets a prompt
