# mcp-glib Development Guide

## Project Overview

mcp-glib is a GLib/GObject implementation of the Model Context Protocol (MCP) for building MCP servers and clients in C.

**Status:** Complete - Full MCP 2025-03-26 protocol implementation
**License:** AGPLv3
**Language:** C (gnu89)
**Dependencies:** glib-2.0, gobject-2.0, gio-2.0, gio-unix-2.0, libdex-1, json-glib-1.0, libsoup-3.0

## Building

```bash
# Build library
make

# Run tests
make test

# Build examples
make examples

# Generate GObject Introspection data
make gir

# Install (default: /usr/local)
sudo make install

# Clean build
make clean
```

## Dependencies (Fedora 43)

```bash
sudo dnf install gcc make pkgconfig glib2-devel json-glib-devel \
    libsoup3-devel libdex-devel gobject-introspection-devel
```

## Cross-Compilation

```bash
# Windows x64 (requires mingw64-* packages)
make WINDOWS=1

# Linux ARM64 (requires sysroot setup first)
make LINUX_ARM64=1

# Custom cross-compiler
make CROSS=x86_64-w64-mingw32

# Show build configuration
make info
```

### Windows Cross-Compilation Packages (Fedora)

```bash
sudo dnf install mingw64-gcc mingw64-glib2 mingw64-json-glib mingw64-pkg-config
```

### ARM64 Linux Cross-Compilation Setup

```bash
# Install cross-compiler and base sysroot
sudo dnf install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu sysroot-aarch64-fc41-glibc

# Add required libraries to sysroot
sudo ./scripts/setup-arm64-sysroot.sh
```

### Platform-Specific Defines

| Define | Effect |
|--------|--------|
| `MCP_NO_LIBSOUP` | Excludes HTTP/WebSocket transports |
| `MCP_NO_STDIO_TRANSPORT` | Excludes stdio transport |

### Windows Build Limitations

mingw cross-compilation excludes transports (stdio, HTTP, WebSocket) due to missing headers/packages in Fedora's mingw repositories. Core MCP types are fully available.

## Directory Structure

```
mcp-glib/
├── mcp/          # Public headers (installed)
├── src/          # Implementation files
├── tests/        # GTest unit tests (13 suites, 170+ tests)
├── examples/     # Example programs
├── docs/         # Documentation (markdown)
└── build/        # Build output (library, tests, GIR)
```

## Code Style

- Use gnu89 C standard
- 4-space indentation with TABs
- All public symbols prefixed with `mcp_` or `Mcp`
- Properties use kebab-case: `input-schema`
- Signals use kebab-case: `tool-called`
- Block comments only: `/* comment */`
- Full GObject Introspection annotations on all public APIs
- Use `g_autoptr()` for automatic cleanup
- Use `g_steal_pointer()` for ownership transfer

## Key Types

### Core Classes (GObject)

| Class | Description |
|-------|-------------|
| `McpServer` | MCP server - exposes tools, resources, prompts |
| `McpClient` | MCP client - connects to servers |
| `McpSession` | Base class for server/client |
| `McpTool` | Tool definition with JSON schema |
| `McpResource` | Resource definition (URI, name, mime-type) |
| `McpResourceTemplate` | URI template for dynamic resources |
| `McpPrompt` | Prompt template with arguments |
| `McpTask` | Long-running operation (experimental) |
| `McpStdioTransport` | Stdio-based transport |

### Boxed Types (Reference Counted)

| Type | Description |
|------|-------------|
| `McpToolResult` | Result from tool invocation |
| `McpResourceContents` | Content from resource read |
| `McpPromptResult` | Result from prompt get |
| `McpPromptMessage` | Message in prompt result |
| `McpImplementation` | Server/client implementation info |

### Enumerations

- `McpTransportState` - Transport connection state
- `McpLogLevel` - Logging levels (debug, info, warning, error, etc.)
- `McpContentType` - Content types (text, image, audio, resource)
- `McpRole` - Message roles (user, assistant)
- `McpTaskStatus` - Task status (working, completed, failed, etc.)
- `McpMessageType` - JSON-RPC message types
- `McpErrorCode` - Error codes (JSON-RPC and MCP-specific)

### Error Handling

- Error domain: `MCP_ERROR`
- Error codes follow JSON-RPC 2.0 and MCP specification
- Use `g_error_matches(error, MCP_ERROR, MCP_ERROR_*)` to check errors

## Handler Function Signatures

```c
/* Tool handler - returns McpToolResult */
typedef McpToolResult *(*McpToolHandler) (McpServer *server,
                                          const gchar *name,
                                          JsonObject *arguments,
                                          gpointer user_data);

/* Resource handler - returns GList of McpResourceContents */
typedef GList *(*McpResourceHandler) (McpServer *server,
                                      const gchar *uri,
                                      gpointer user_data);

/* Prompt handler - returns McpPromptResult */
typedef McpPromptResult *(*McpPromptHandler) (McpServer *server,
                                              const gchar *name,
                                              GHashTable *arguments,
                                              gpointer user_data);
```

## Testing

Tests use GLib's GTest framework:

```bash
# Run all tests
make test

# Run specific test with verbose output
./build/test-enums -v

# Run specific test case
./build/test-enums -p /mcp/enums/log-level/to-string

# List test cases
./build/test-server -l
```

### Test Suites

| Test | Description |
|------|-------------|
| `test-enums` | Enumeration serialization |
| `test-error` | Error codes and domain |
| `test-content` | Content types |
| `test-tool` | Tool definitions |
| `test-resource` | Resources and templates |
| `test-prompt` | Prompts and arguments |
| `test-task` | Task lifecycle |
| `test-message` | JSON-RPC messages |
| `test-capabilities` | Capability negotiation |
| `test-transport-mock` | Mock transport |
| `test-server` | Server with mock transport |
| `test-client` | Client with mock transport |
| `test-integration` | End-to-end server+client |

## Example Programs

| Example | Description |
|---------|-------------|
| `simple-server` | Echo tool, readme resource, greeting prompt |
| `simple-client` | Connects to server, exercises all features |
| `calculator-server` | Math tools (add, subtract, multiply, divide, sqrt, power) |
| `filesystem-server` | Serves files using resource templates |

## Protocol Version

- Latest: `MCP_PROTOCOL_VERSION` = "2025-11-25"
- Default negotiated: `MCP_DEFAULT_NEGOTIATED_VERSION` = "2025-03-26"

## Common Patterns

### Creating a Server

```c
g_autoptr(McpServer) server = mcp_server_new ("my-server", "1.0.0");
mcp_server_set_instructions (server, "...");

/* Add tool */
g_autoptr(McpTool) tool = mcp_tool_new ("name", "description");
mcp_server_add_tool (server, tool, handler, user_data, NULL);

/* Set transport and start */
g_autoptr(McpStdioTransport) transport = mcp_stdio_transport_new ();
mcp_server_set_transport (server, MCP_TRANSPORT (transport));
mcp_server_start_async (server, NULL, NULL, NULL);
```

### Creating a Client

```c
client = mcp_client_new ("my-client", "1.0.0");

/* Spawn server subprocess */
g_autoptr(McpStdioTransport) transport =
    mcp_stdio_transport_new_subprocess_simple ("./server", &error);
mcp_client_set_transport (client, MCP_TRANSPORT (transport));
mcp_client_connect_async (client, NULL, on_connected, NULL);
```

### Async Pattern

```c
/* Start async operation */
mcp_client_call_tool_async (client, "tool", args, NULL, on_complete, user_data);

/* In callback */
static void
on_complete (GObject *source, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GError) error = NULL;
    McpToolResult *tool_result;

    tool_result = mcp_client_call_tool_finish (MCP_CLIENT (source), result, &error);
    if (error != NULL)
    {
        /* Handle error */
        return;
    }

    /* Process result */
    mcp_tool_result_unref (tool_result);
}
```

## GObject Introspection

Generate GIR and typelib:

```bash
make gir
# Creates: build/Mcp-1.0.gir, build/Mcp-1.0.typelib
```

Use from Python:

```python
import gi
gi.require_version('Mcp', '1.0')
from gi.repository import Mcp
```
