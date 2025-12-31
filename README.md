# mcp-glib

A GLib/GObject implementation of the [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) for building MCP servers and clients in C.

## Features

- **MCP Server Support** - Expose tools, resources, and prompts to AI applications
- **MCP Client Support** - Connect to and interact with MCP servers
- **Tasks API** - Experimental support for long-running tool operations
- **Stdio Transport** - Process-based communication via stdin/stdout
- **GObject Introspection** - First-class support for language bindings (Python, JavaScript, etc.)
- **Async Operations** - Built on libdex for composable async futures
- **Cross-Platform** - Windows DLL via mingw cross-compilation

## Status

**Complete** - Full MCP 2025-03-26 protocol implementation with:
- Tools, Resources, Resource Templates, and Prompts
- Server and Client classes with async operations
- Tasks API for long-running operations (experimental)
- Stdio transport with subprocess spawning
- GIR/typelib generation for language bindings
- Comprehensive test suite (170+ tests)

## Requirements

- GCC with gnu89 support
- GLib 2.0 / GObject 2.0 / GIO 2.0 / GIO-Unix 2.0
- json-glib 1.0
- libsoup 3.0
- libdex 1.0

### Fedora 43

```bash
sudo dnf install gcc make pkgconfig glib2-devel json-glib-devel \
    libsoup3-devel libdex-devel gobject-introspection-devel
```

## Building

```bash
make              # Build library
make test         # Run tests
make examples     # Build examples
make gir          # Generate GIR and typelib
make install      # Install to /usr/local
```

### Cross-Compilation

```bash
make WINDOWS=1      # Windows x64 DLL (via mingw)
make LINUX_ARM64=1  # Linux ARM64
```

See [docs/building.md](docs/building.md#cross-compilation) for details.

## Quick Example

### Simple MCP Server

```c
#include <mcp/mcp.h>

static GMainLoop *main_loop = NULL;

/* Tool handler - called when client invokes the tool */
static McpToolResult *
echo_handler (McpServer  *server,
              const gchar *name,
              JsonObject  *arguments,
              gpointer     user_data)
{
    McpToolResult *result;
    const gchar *message = NULL;

    if (arguments != NULL && json_object_has_member (arguments, "message"))
    {
        message = json_object_get_string_member (arguments, "message");
    }

    if (message == NULL)
    {
        message = "(no message)";
    }

    result = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_text (result, message);

    return result;
}

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
    g_autoptr(McpTool) echo_tool = NULL;

    main_loop = g_main_loop_new (NULL, FALSE);

    /* Create server */
    server = mcp_server_new ("my-server", "1.0.0");
    mcp_server_set_instructions (server, "Use the echo tool to echo messages.");

    g_signal_connect (server, "client-disconnected",
                      G_CALLBACK (on_client_disconnected), NULL);

    /* Add a tool */
    echo_tool = mcp_tool_new ("echo", "Echoes back the message");
    mcp_server_add_tool (server, echo_tool, echo_handler, NULL, NULL);

    /* Create stdio transport and start */
    transport = mcp_stdio_transport_new ();
    mcp_server_set_transport (server, MCP_TRANSPORT (transport));
    mcp_server_start_async (server, NULL, NULL, NULL);

    g_main_loop_run (main_loop);
    g_main_loop_unref (main_loop);

    return 0;
}
```

### Simple MCP Client

```c
#include <mcp/mcp.h>

static GMainLoop *main_loop = NULL;
static McpClient *client = NULL;

static void
on_tool_called (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    McpToolResult *tool_result;

    tool_result = mcp_client_call_tool_finish (MCP_CLIENT (source), result, &error);

    if (error != NULL)
    {
        g_printerr ("Tool call failed: %s\n", error->message);
    }
    else
    {
        JsonArray *content = mcp_tool_result_get_content (tool_result);
        guint i;

        for (i = 0; i < json_array_get_length (content); i++)
        {
            JsonObject *item = json_array_get_object_element (content, i);
            const gchar *type = json_object_get_string_member (item, "type");

            if (g_strcmp0 (type, "text") == 0)
            {
                g_print ("Result: %s\n", json_object_get_string_member (item, "text"));
            }
        }

        mcp_tool_result_unref (tool_result);
    }

    g_main_loop_quit (main_loop);
}

static void
on_connected (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(JsonObject) args = NULL;

    if (!mcp_client_connect_finish (MCP_CLIENT (source), result, &error))
    {
        g_printerr ("Connection failed: %s\n", error->message);
        g_main_loop_quit (main_loop);
        return;
    }

    g_print ("Connected!\n");

    /* Call the echo tool */
    args = json_object_new ();
    json_object_set_string_member (args, "message", "Hello, MCP!");
    mcp_client_call_tool_async (client, "echo", args, NULL, on_tool_called, NULL);
}

int
main (int argc, char *argv[])
{
    g_autoptr(McpStdioTransport) transport = NULL;
    g_autoptr(GError) error = NULL;

    main_loop = g_main_loop_new (NULL, FALSE);

    /* Create client */
    client = mcp_client_new ("my-client", "1.0.0");

    /* Spawn server as subprocess */
    transport = mcp_stdio_transport_new_subprocess_simple ("./my-server", &error);
    if (transport == NULL)
    {
        g_printerr ("Failed to spawn server: %s\n", error->message);
        return 1;
    }

    mcp_client_set_transport (client, MCP_TRANSPORT (transport));
    mcp_client_connect_async (client, NULL, on_connected, NULL);

    g_main_loop_run (main_loop);

    g_object_unref (client);
    g_main_loop_unref (main_loop);

    return 0;
}
```

## Examples

The `examples/` directory contains working example programs:

- **simple-server.c** - Minimal server with echo tool, readme resource, and greeting prompt
- **simple-client.c** - Client that connects to a server and exercises all features
- **calculator-server.c** - Calculator with add, subtract, multiply, divide, sqrt, power tools
- **filesystem-server.c** - Serves files from a directory using resource templates

Build and run examples:

```bash
make examples
./build/simple-client ./build/simple-server
./build/simple-client ./build/calculator-server
./build/simple-client ./build/filesystem-server /path/to/directory
```

## Testing

Run the test suite:

```bash
make test
```

Tests include:
- Unit tests for all core types (enums, content, tools, resources, prompts, tasks)
- Protocol tests for message encoding/decoding
- Server and client tests with mock transport
- Integration tests with linked transport pair

## API Overview

### Core Types

| Type | Description |
|------|-------------|
| `McpServer` | MCP server - exposes tools, resources, prompts |
| `McpClient` | MCP client - connects to servers |
| `McpTool` | Tool definition with JSON schema |
| `McpResource` | Resource definition (URI, name, mime-type) |
| `McpResourceTemplate` | URI template for dynamic resources |
| `McpPrompt` | Prompt template with arguments |
| `McpTask` | Long-running operation (experimental) |

### Transport Types

| Type | Description |
|------|-------------|
| `McpTransport` | Abstract transport interface |
| `McpStdioTransport` | Stdio-based transport |

### Result Types (Boxed)

| Type | Description |
|------|-------------|
| `McpToolResult` | Result from tool invocation |
| `McpResourceContents` | Content from resource read |
| `McpPromptResult` | Result from prompt get |
| `McpPromptMessage` | Message in prompt result |

## Language Bindings

Generate GObject Introspection files:

```bash
make gir
```

This creates:
- `build/Mcp-1.0.gir` - GIR XML file
- `build/Mcp-1.0.typelib` - Compiled typelib

### Python Example

```python
import gi
gi.require_version('Mcp', '1.0')
from gi.repository import Mcp, GLib

# Create a server
server = Mcp.Server.new("python-server", "1.0.0")
# ... add tools, resources, prompts
```

## License

AGPL-3.0-or-later

Copyright (C) 2025 Copyleft Games

## Documentation

See [docs/](docs/) for detailed documentation:

- [Building](docs/building.md) - Build instructions and dependencies
- [Server Guide](docs/server-guide.md) - How to create MCP servers
- [Client Guide](docs/client-guide.md) - How to create MCP clients
- [API Reference](docs/api-reference.md) - Complete API documentation

## References

- [MCP Specification](https://spec.modelcontextprotocol.io/)
- [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification)
- [GObject Manual](https://docs.gtk.org/gobject/)
- [libdex Documentation](https://gnome.pages.gitlab.gnome.org/libdex/)
