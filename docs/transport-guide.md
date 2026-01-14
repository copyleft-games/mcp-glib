# Transport Guide

This guide covers the transport layer in mcp-glib, including the abstract interface and concrete implementations.

## Overview

Transports handle the low-level communication between MCP clients and servers. mcp-glib provides:

- **McpTransport** - Abstract interface defining transport operations
- **McpStdioTransport** - Communication via stdin/stdout (process-based)
- **McpHttpTransport** - HTTP client transport (POST + SSE)
- **McpHttpServerTransport** - HTTP server transport (accepts POST + SSE)
- **McpWebSocketTransport** - WebSocket client transport
- **McpWebSocketServerTransport** - WebSocket server transport

### Transport Comparison

| Transport | Direction | Protocol | Use Case |
|-----------|-----------|----------|----------|
| `McpStdioTransport` | Both | stdin/stdout | Local subprocess |
| `McpHttpTransport` | Client | HTTP POST + SSE | Connect to HTTP server |
| `McpHttpServerTransport` | Server | HTTP POST + SSE | Accept HTTP clients |
| `McpWebSocketTransport` | Client | WebSocket | Connect to WS server |
| `McpWebSocketServerTransport` | Server | WebSocket | Accept WS clients |

## McpTransport Interface

All transports implement the `McpTransport` interface:

```c
/* Connect to the remote endpoint */
void mcp_transport_connect_async (McpTransport *transport,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);
gboolean mcp_transport_connect_finish (McpTransport *transport,
                                       GAsyncResult *result,
                                       GError **error);

/* Disconnect */
void mcp_transport_disconnect_async (McpTransport *transport,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data);
gboolean mcp_transport_disconnect_finish (McpTransport *transport,
                                          GAsyncResult *result,
                                          GError **error);

/* Send a message */
void mcp_transport_send_message_async (McpTransport *transport,
                                       JsonNode *message,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data);
gboolean mcp_transport_send_message_finish (McpTransport *transport,
                                            GAsyncResult *result,
                                            GError **error);

/* Get current state */
McpTransportState mcp_transport_get_state (McpTransport *transport);
```

### Transport States

```c
typedef enum {
    MCP_TRANSPORT_STATE_DISCONNECTED,   /* Not connected */
    MCP_TRANSPORT_STATE_CONNECTING,     /* Connection in progress */
    MCP_TRANSPORT_STATE_CONNECTED,      /* Ready to send/receive */
    MCP_TRANSPORT_STATE_DISCONNECTING,  /* Disconnection in progress */
    MCP_TRANSPORT_STATE_ERROR           /* Error occurred */
} McpTransportState;
```

### Signals

| Signal | Description |
|--------|-------------|
| `message-received` | A message was received from the remote endpoint |
| `state-changed` | Transport state changed |
| `error` | An error occurred |

---

## Stdio Transport

The stdio transport communicates via stdin/stdout, typically used with subprocess servers.

### Server-Side Usage

For servers, create a transport that uses the process's stdin/stdout:

```c
g_autoptr(McpStdioTransport) transport = mcp_stdio_transport_new ();
mcp_server_set_transport (server, MCP_TRANSPORT (transport));
```

### Client-Side Usage

For clients, spawn a server as a subprocess:

```c
g_autoptr(GError) error = NULL;
g_autoptr(McpStdioTransport) transport = NULL;

/* Simple version - single command string */
transport = mcp_stdio_transport_new_subprocess_simple ("./my-server --arg", &error);
if (transport == NULL)
{
    g_printerr ("Failed to spawn: %s\n", error->message);
    return;
}

mcp_client_set_transport (client, MCP_TRANSPORT (transport));
```

### With Custom Streams

For advanced use cases, provide custom streams:

```c
GInputStream *input = ...;   /* Read from server */
GOutputStream *output = ...; /* Write to server */

transport = mcp_stdio_transport_new_with_streams (input, output);
```

### Message Framing

Stdio transport uses newline-delimited JSON (NDJSON):
- Each message is a complete JSON object on a single line
- Messages are separated by `\n`
- No length prefix required

---

## HTTP Transport

The HTTP transport uses HTTP POST for client-to-server messages and Server-Sent Events (SSE) for server-to-client messages.

### Creating HTTP Transport

```c
g_autoptr(McpHttpTransport) transport = NULL;

transport = mcp_http_transport_new ("https://mcp.example.com/api");
```

### Configuration

```c
/* Set authentication token */
mcp_http_transport_set_auth_token (transport, "Bearer my-token");

/* Set custom endpoint paths */
mcp_http_transport_set_message_endpoint (transport, "/mcp/message");
mcp_http_transport_set_sse_endpoint (transport, "/mcp/events");

/* Configure timeouts */
mcp_http_transport_set_timeout (transport, 30);  /* seconds */

/* Configure reconnection */
mcp_http_transport_set_reconnect_delay (transport, 1000);  /* ms */
mcp_http_transport_set_max_reconnect_delay (transport, 30000);  /* ms */
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `base-url` | gchar* | Base URL for the MCP endpoint |
| `auth-token` | gchar* | Authorization token |
| `message-endpoint` | gchar* | Path for POST messages (default: `/message`) |
| `sse-endpoint` | gchar* | Path for SSE events (default: `/sse`) |
| `timeout` | guint | Request timeout in seconds |
| `reconnect-delay` | guint | Initial reconnect delay in ms |
| `max-reconnect-delay` | guint | Maximum reconnect delay in ms |

### How It Works

1. **Client→Server**: Messages sent via HTTP POST to `{base-url}{message-endpoint}`
2. **Server→Client**: Messages received via SSE connection to `{base-url}{sse-endpoint}`
3. **Session Management**: Session ID returned in initialize response is used for subsequent requests

### Server-Side Considerations

When implementing an HTTP-based MCP server (not using mcp-glib's server):
- POST endpoint receives JSON-RPC messages
- SSE endpoint streams responses and notifications
- Include session ID in responses for client tracking

---

## WebSocket Transport

The WebSocket transport provides full-duplex communication over a single connection.

### Creating WebSocket Transport

```c
g_autoptr(McpWebSocketTransport) transport = NULL;

transport = mcp_websocket_transport_new ("wss://mcp.example.com/ws");
```

### Configuration

```c
/* Set authentication token (sent as Sec-WebSocket-Protocol or header) */
mcp_websocket_transport_set_auth_token (transport, "my-token");

/* Set origin header */
mcp_websocket_transport_set_origin (transport, "https://myapp.example.com");

/* Set subprotocols */
const gchar *protocols[] = { "mcp", NULL };
mcp_websocket_transport_set_protocols (transport, protocols);

/* Configure keepalive */
mcp_websocket_transport_set_keepalive_interval (transport, 30);  /* seconds */

/* Configure reconnection */
mcp_websocket_transport_set_reconnect (transport, TRUE);
mcp_websocket_transport_set_max_reconnect_attempts (transport, 5);
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `uri` | gchar* | WebSocket URI (ws:// or wss://) |
| `auth-token` | gchar* | Authentication token |
| `origin` | gchar* | Origin header value |
| `protocols` | GStrv | Subprotocol list |
| `keepalive-interval` | guint | Ping interval in seconds (0 = disabled) |
| `reconnect` | gboolean | Auto-reconnect on disconnect |
| `max-reconnect-attempts` | gint | Max reconnect attempts (-1 = unlimited) |

### Custom SoupSession

For advanced HTTP/proxy configuration:

```c
g_autoptr(SoupSession) session = soup_session_new ();
/* Configure session... */

transport = mcp_websocket_transport_new_with_session (
    "wss://mcp.example.com/ws", session);
```

---

## HTTP Server Transport

The HTTP server transport allows an MCP server to accept connections from clients over HTTP using POST + Server-Sent Events.

### Creating HTTP Server Transport

```c
g_autoptr(McpHttpServerTransport) transport = NULL;

/* Listen on port 8080 */
transport = mcp_http_server_transport_new (8080);

/* Or with specific host binding */
transport = mcp_http_server_transport_new_full ("127.0.0.1", 8080);
```

### Configuration

```c
/* Set custom endpoint paths */
mcp_http_server_transport_set_post_path (transport, "/mcp");
mcp_http_server_transport_set_sse_path (transport, "/events");

/* Enable authentication */
mcp_http_server_transport_set_require_auth (transport, TRUE);
mcp_http_server_transport_set_auth_token (transport, "secret-token");

/* Enable TLS (HTTPS) */
g_autoptr(GTlsCertificate) cert = g_tls_certificate_new_from_files (
    "cert.pem", "key.pem", &error);
mcp_http_server_transport_set_tls_certificate (transport, cert);
```

### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `port` | guint | 0 | Port to listen on (0 = auto-assign) |
| `host` | gchar* | NULL | Host/address to bind (NULL = all interfaces) |
| `post-path` | gchar* | "/" | Path for POST requests |
| `sse-path` | gchar* | "/sse" | Path for SSE connections |
| `session-id` | gchar* | (auto) | Current session ID (read-only) |
| `require-auth` | gboolean | FALSE | Require Bearer token |
| `auth-token` | gchar* | NULL | Expected Bearer token |
| `tls-certificate` | GTlsCertificate* | NULL | TLS certificate for HTTPS |

### Usage with McpServer

```c
g_autoptr(McpServer) server = mcp_server_new ("my-server", "1.0.0");
g_autoptr(McpHttpServerTransport) transport = mcp_http_server_transport_new (8080);

mcp_server_set_transport (server, MCP_TRANSPORT (transport));
mcp_server_start_async (server, NULL, on_started, NULL);
```

### Protocol Details

1. Client connects to SSE endpoint (GET `/sse`)
2. Server generates session ID, sends it in `Mcp-Session-Id` header
3. Client sends JSON-RPC messages via POST to `/` with `Mcp-Session-Id` header
4. Server sends responses/notifications via SSE stream

### Example

See `examples/http-server.c` for a complete example.

---

## WebSocket Server Transport

The WebSocket server transport allows an MCP server to accept WebSocket connections for bidirectional communication.

### Creating WebSocket Server Transport

```c
g_autoptr(McpWebSocketServerTransport) transport = NULL;

/* Listen on port 8081 */
transport = mcp_websocket_server_transport_new (8081);

/* Or with host and custom path */
transport = mcp_websocket_server_transport_new_full ("0.0.0.0", 8081, "/mcp");
```

### Configuration

```c
/* Set WebSocket path */
mcp_websocket_server_transport_set_path (transport, "/ws");

/* Set accepted subprotocols */
const gchar * const protocols[] = { "mcp", "json-rpc", NULL };
mcp_websocket_server_transport_set_protocols (transport, protocols);

/* Set required origin */
mcp_websocket_server_transport_set_origin (transport, "https://myapp.com");

/* Enable authentication */
mcp_websocket_server_transport_set_require_auth (transport, TRUE);
mcp_websocket_server_transport_set_auth_token (transport, "secret-token");

/* Configure keepalive (ping frames) */
mcp_websocket_server_transport_set_keepalive_interval (transport, 30);  /* seconds */

/* Enable TLS (WSS) */
g_autoptr(GTlsCertificate) cert = g_tls_certificate_new_from_files (
    "cert.pem", "key.pem", &error);
mcp_websocket_server_transport_set_tls_certificate (transport, cert);
```

### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `port` | guint | 0 | Port to listen on (0 = auto-assign) |
| `host` | gchar* | NULL | Host/address to bind (NULL = all interfaces) |
| `path` | gchar* | "/" | WebSocket endpoint path |
| `protocols` | GStrv | NULL | Accepted subprotocols |
| `origin` | gchar* | NULL | Required origin (NULL = any) |
| `require-auth` | gboolean | FALSE | Require Bearer token |
| `auth-token` | gchar* | NULL | Expected Bearer token |
| `keepalive-interval` | guint | 30 | Ping interval in seconds (0 = disabled) |
| `tls-certificate` | GTlsCertificate* | NULL | TLS certificate for WSS |

### Usage with McpServer

```c
g_autoptr(McpServer) server = mcp_server_new ("my-server", "1.0.0");
g_autoptr(McpWebSocketServerTransport) transport =
    mcp_websocket_server_transport_new (8081);

mcp_server_set_transport (server, MCP_TRANSPORT (transport));
mcp_server_start_async (server, NULL, on_started, NULL);
```

### Single-Client Model

Both HTTP and WebSocket server transports follow a single-client model:
- Each transport instance handles one client connection at a time
- Additional connection attempts are rejected until the current client disconnects
- For multi-client scenarios, use multiple `McpServer` instances

### Getting Actual Port

When using port 0 (auto-assign), get the actual port after connecting:

```c
static void
on_server_started (GObject *source, GAsyncResult *result, gpointer user_data)
{
    McpWebSocketServerTransport *transport = user_data;
    guint actual_port = mcp_websocket_server_transport_get_actual_port (transport);
    g_print ("Server listening on port %u\n", actual_port);
}
```

### Example

See `examples/websocket-server.c` for a complete example.

---

## Choosing a Transport

### Client Transports

| Transport | Use Case | Pros | Cons |
|-----------|----------|------|------|
| **Stdio** | Local subprocess servers | Simple, no network | Single process only |
| **HTTP** | Stateless APIs, load balancing | HTTP infrastructure | Higher latency |
| **WebSocket** | Real-time, bidirectional | Low latency, full-duplex | Requires WS support |

### Server Transports

| Transport | Use Case | Pros | Cons |
|-----------|----------|------|------|
| **HTTP Server** | REST-like APIs, browser clients | Standard HTTP, SSE fallback | Unidirectional SSE |
| **WebSocket Server** | Real-time apps, low latency | Full-duplex, efficient | Requires WS support |

### Recommendations

**For Clients:**
- **Local tools/servers**: Use `McpStdioTransport` with subprocess
- **Cloud-hosted MCP**: Use `McpHttpTransport` or `McpWebSocketTransport`
- **Real-time updates**: Prefer `McpWebSocketTransport`
- **Behind proxies**: `McpHttpTransport` often works better

**For Servers:**
- **Network-accessible server**: Use `McpHttpServerTransport` or `McpWebSocketServerTransport`
- **Browser-based clients**: Use `McpHttpServerTransport` (SSE works everywhere)
- **Real-time bidirectional**: Use `McpWebSocketServerTransport`
- **Local subprocess**: Use `McpStdioTransport` (works for both client and server)

---

## Implementing Custom Transports

To create a custom transport, implement the `McpTransport` interface:

```c
/* In header */
#define MY_TYPE_TRANSPORT (my_transport_get_type ())
G_DECLARE_FINAL_TYPE (MyTransport, my_transport, MY, TRANSPORT, GObject)

/* In source */
static void
my_transport_iface_init (McpTransportInterface *iface)
{
    iface->connect_async = my_transport_connect_async;
    iface->connect_finish = my_transport_connect_finish;
    iface->disconnect_async = my_transport_disconnect_async;
    iface->disconnect_finish = my_transport_disconnect_finish;
    iface->send_message_async = my_transport_send_message_async;
    iface->send_message_finish = my_transport_send_message_finish;
    iface->get_state = my_transport_get_state;
}

G_DEFINE_TYPE_WITH_CODE (MyTransport, my_transport, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MCP_TYPE_TRANSPORT,
                                                my_transport_iface_init))
```

### Required Methods

1. **connect_async/finish**: Establish connection, transition to CONNECTED state
2. **disconnect_async/finish**: Close connection, transition to DISCONNECTED state
3. **send_message_async/finish**: Send a JSON message to the remote endpoint
4. **get_state**: Return current `McpTransportState`

### Required Signals

Emit these signals appropriately:
- `message-received` when a message arrives (with `JsonNode*` parameter)
- `state-changed` when state changes (with old and new `McpTransportState`)
- `error` when an error occurs (with `GError*` parameter)

### Helper Functions

```c
/* Emit message-received signal */
void mcp_transport_emit_message_received (McpTransport *transport,
                                          JsonNode *message);

/* Emit state-changed signal */
void mcp_transport_emit_state_changed (McpTransport *transport,
                                       McpTransportState old_state,
                                       McpTransportState new_state);

/* Emit error signal */
void mcp_transport_emit_error (McpTransport *transport,
                               GError *error);
```

---

## Error Handling

Transport errors use the `MCP_ERROR` domain:

```c
if (g_error_matches (error, MCP_ERROR, MCP_ERROR_CONNECTION_CLOSED))
{
    g_print ("Connection was closed\n");
}
else if (g_error_matches (error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR))
{
    g_print ("Transport error: %s\n", error->message);
}
else if (g_error_matches (error, MCP_ERROR, MCP_ERROR_TIMEOUT))
{
    g_print ("Operation timed out\n");
}
```

### Reconnection

HTTP and WebSocket transports support automatic reconnection:

```c
/* Enable reconnection */
mcp_websocket_transport_set_reconnect (transport, TRUE);

/* Listen for reconnection events */
g_signal_connect (transport, "state-changed",
                  G_CALLBACK (on_state_changed), NULL);

static void
on_state_changed (McpTransport      *transport,
                  McpTransportState  old_state,
                  McpTransportState  new_state,
                  gpointer           user_data)
{
    if (old_state == MCP_TRANSPORT_STATE_ERROR &&
        new_state == MCP_TRANSPORT_STATE_CONNECTING)
    {
        g_print ("Reconnecting...\n");
    }
    else if (new_state == MCP_TRANSPORT_STATE_CONNECTED)
    {
        g_print ("Connected!\n");
    }
}
```
