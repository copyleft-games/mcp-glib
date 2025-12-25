# MCP Server Guide

This guide explains how to create MCP servers using mcp-glib.

## Overview

An MCP server exposes capabilities to AI clients:
- **Tools** - Functions the AI can call
- **Resources** - Data the AI can read
- **Prompts** - Templates for AI interactions

## Basic Server Structure

```c
#include <mcp/mcp.h>

static GMainLoop *main_loop = NULL;

static void
on_client_disconnected (McpServer *server, gpointer user_data)
{
    g_main_loop_quit (main_loop);
}

int
main (int argc, char *argv[])
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpStdioTransport) transport = NULL;

    main_loop = g_main_loop_new (NULL, FALSE);

    /* Create server with name and version */
    server = mcp_server_new ("my-server", "1.0.0");

    /* Optional: Set instructions for the AI */
    mcp_server_set_instructions (server, "Instructions for the AI...");

    /* Connect disconnect signal to quit */
    g_signal_connect (server, "client-disconnected",
                      G_CALLBACK (on_client_disconnected), NULL);

    /* Add tools, resources, prompts here */

    /* Create stdio transport and start */
    transport = mcp_stdio_transport_new ();
    mcp_server_set_transport (server, MCP_TRANSPORT (transport));
    mcp_server_start_async (server, NULL, NULL, NULL);

    /* Run until client disconnects */
    g_main_loop_run (main_loop);
    g_main_loop_unref (main_loop);

    return 0;
}
```

## Adding Tools

Tools are functions that AI clients can call.

### Defining a Tool

```c
/* Tool handler function */
static McpToolResult *
greet_handler (McpServer   *server,
               const gchar *name,
               JsonObject  *arguments,
               gpointer     user_data)
{
    McpToolResult *result;
    const gchar *person = NULL;
    g_autofree gchar *greeting = NULL;

    /* Get arguments */
    if (arguments != NULL && json_object_has_member (arguments, "name"))
    {
        person = json_object_get_string_member (arguments, "name");
    }

    if (person == NULL)
    {
        /* Return error result */
        result = mcp_tool_result_new (TRUE);  /* TRUE = is_error */
        mcp_tool_result_add_text (result, "Missing required argument 'name'");
        return result;
    }

    /* Create greeting */
    greeting = g_strdup_printf ("Hello, %s!", person);

    /* Return success result */
    result = mcp_tool_result_new (FALSE);  /* FALSE = success */
    mcp_tool_result_add_text (result, greeting);

    return result;
}

/* In main() or setup function */
{
    g_autoptr(McpTool) tool = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) schema = NULL;

    /* Create tool */
    tool = mcp_tool_new ("greet", "Greet a person by name");

    /* Define input schema */
    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "object");
    json_builder_set_member_name (builder, "properties");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "name");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "string");
    json_builder_set_member_name (builder, "description");
    json_builder_add_string_value (builder, "The name of the person to greet");
    json_builder_end_object (builder);
    json_builder_end_object (builder);
    json_builder_set_member_name (builder, "required");
    json_builder_begin_array (builder);
    json_builder_add_string_value (builder, "name");
    json_builder_end_array (builder);
    json_builder_end_object (builder);

    schema = json_builder_get_root (builder);
    mcp_tool_set_input_schema (tool, schema);

    /* Register with server */
    mcp_server_add_tool (server, tool, greet_handler, NULL, NULL);
}
```

### Tool Result Types

Tools can return different content types:

```c
/* Text content */
mcp_tool_result_add_text (result, "Hello, world!");

/* Image content (base64 encoded) */
mcp_tool_result_add_image (result, base64_data, "image/png");

/* Multiple content items */
mcp_tool_result_add_text (result, "Here is the chart:");
mcp_tool_result_add_image (result, chart_base64, "image/png");
```

### Error Results

```c
/* Create error result */
result = mcp_tool_result_new (TRUE);  /* TRUE = is_error */
mcp_tool_result_add_text (result, "Something went wrong: ...");
return result;
```

## Adding Resources

Resources are data that AI clients can read.

### Static Resources

```c
/* Resource handler */
static GList *
readme_handler (McpServer   *server,
                const gchar *uri,
                gpointer     user_data)
{
    McpResourceContents *contents;
    const gchar *text = "# My Server\n\nThis is the readme.";

    contents = mcp_resource_contents_new_text (uri, text, "text/markdown");

    return g_list_append (NULL, contents);
}

/* In main() */
{
    g_autoptr(McpResource) resource = NULL;

    resource = mcp_resource_new ("file:///readme", "README");
    mcp_resource_set_description (resource, "Server documentation");
    mcp_resource_set_mime_type (resource, "text/markdown");

    mcp_server_add_resource (server, resource, readme_handler, NULL, NULL);
}
```

### Resource Templates

Resource templates allow dynamic URIs with parameters:

```c
/* Template handler - handles any matching URI */
static GList *
file_handler (McpServer   *server,
              const gchar *uri,
              gpointer     user_data)
{
    McpResourceContents *contents;
    g_autofree gchar *path = NULL;
    g_autofree gchar *file_contents = NULL;
    g_autoptr(GError) error = NULL;

    /* Extract path from URI (e.g., "file:///docs/intro.md" -> "docs/intro.md") */
    if (!g_str_has_prefix (uri, "file:///"))
    {
        return NULL;
    }
    path = g_strdup (uri + 8);

    /* Read file */
    if (!g_file_get_contents (path, &file_contents, NULL, &error))
    {
        g_warning ("Failed to read %s: %s", path, error->message);
        return NULL;
    }

    contents = mcp_resource_contents_new_text (uri, file_contents, "text/plain");

    return g_list_append (NULL, contents);
}

/* In main() */
{
    g_autoptr(McpResourceTemplate) template = NULL;

    template = mcp_resource_template_new ("file:///{path}", "File Access");
    mcp_resource_template_set_description (template, "Access files by path");
    mcp_resource_template_set_mime_type (template, "text/plain");

    mcp_server_add_resource_template (server, template, file_handler, NULL, NULL);
}
```

### Binary Resources

```c
static GList *
image_handler (McpServer   *server,
               const gchar *uri,
               gpointer     user_data)
{
    McpResourceContents *contents;
    g_autofree gchar *data = NULL;
    gsize length;
    g_autofree gchar *base64 = NULL;

    /* Read binary file */
    g_file_get_contents ("image.png", &data, &length, NULL);

    /* Encode as base64 */
    base64 = g_base64_encode ((guchar *) data, length);

    /* Create blob contents */
    contents = mcp_resource_contents_new_blob (uri, base64, "image/png");

    return g_list_append (NULL, contents);
}
```

## Adding Prompts

Prompts are templates for AI interactions.

```c
/* Prompt handler */
static McpPromptResult *
greeting_handler (McpServer   *server,
                  const gchar *name,
                  GHashTable  *arguments,
                  gpointer     user_data)
{
    McpPromptResult *result;
    McpPromptMessage *message;
    const gchar *user_name = NULL;
    g_autofree gchar *text = NULL;

    /* Get arguments from hash table */
    if (arguments != NULL)
    {
        user_name = g_hash_table_lookup (arguments, "name");
    }

    if (user_name == NULL)
    {
        user_name = "friend";
    }

    /* Create result with description */
    result = mcp_prompt_result_new ("A friendly greeting");

    /* Add message(s) */
    text = g_strdup_printf ("Hello, %s! How can I help you today?", user_name);
    message = mcp_prompt_message_new (MCP_ROLE_ASSISTANT);
    mcp_prompt_message_add_text (message, text);
    mcp_prompt_result_add_message (result, message);
    mcp_prompt_message_unref (message);

    return result;
}

/* In main() */
{
    g_autoptr(McpPrompt) prompt = NULL;

    prompt = mcp_prompt_new ("greeting", "Generate a friendly greeting");

    /* Add optional argument */
    mcp_prompt_add_argument_full (prompt, "name", "Name to greet", FALSE);

    mcp_server_add_prompt (server, prompt, greeting_handler, NULL, NULL);
}
```

## Server Signals

### client-initialized

Emitted when a client connects and completes the handshake:

```c
static void
on_client_initialized (McpServer *server, gpointer user_data)
{
    g_message ("Client connected");
}

g_signal_connect (server, "client-initialized",
                  G_CALLBACK (on_client_initialized), NULL);
```

### client-disconnected

Emitted when the client disconnects:

```c
static void
on_client_disconnected (McpServer *server, gpointer user_data)
{
    g_message ("Client disconnected");
    g_main_loop_quit (main_loop);
}

g_signal_connect (server, "client-disconnected",
                  G_CALLBACK (on_client_disconnected), NULL);
```

## Dynamic Updates

Notify clients when capabilities change:

```c
/* Add a new tool dynamically */
mcp_server_add_tool (server, new_tool, handler, NULL, NULL);

/* Notify clients that tools changed */
mcp_server_notify_tools_changed (server);

/* Similarly for resources and prompts */
mcp_server_notify_resources_changed (server);
mcp_server_notify_prompts_changed (server);
```

## Server Capabilities

Configure server capabilities:

```c
McpServerCapabilities *caps = mcp_server_get_capabilities (server);

/* Enable experimental features */
mcp_server_capabilities_set_experimental (caps, "tasks", TRUE);
```

## Logging

Send log messages to connected clients:

```c
mcp_server_emit_log (server, MCP_LOG_LEVEL_INFO,
                     "Processing request...", "my-logger");
```

## Complete Example

See `examples/simple-server.c` for a complete working example with:
- Echo tool
- README resource
- Greeting prompt

See `examples/calculator-server.c` for a more complex example with:
- Multiple mathematical tools
- Error handling
- JSON schema validation

See `examples/filesystem-server.c` for:
- Resource templates
- File serving
- Path validation
