# Transport Guide

This guide covers the transport layer in mcp-glib, including the abstract interface and concrete implementations.

## Overview

Transports handle the low-level communication between MCP clients and servers. mcp-glib provides:

- **McpTransport** - Abstract interface defining transport operations
- **McpStdioTransport** - Communication via stdin/stdout (process-based)
- **McpHttpTransport** - HTTP POST + Server-Sent Events
- **McpWebSocketTransport** - Full-duplex WebSocket connection

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

## Choosing a Transport

| Transport | Use Case | Pros | Cons |
|-----------|----------|------|------|
| **Stdio** | Local subprocess servers | Simple, no network | Single process only |
| **HTTP** | Stateless APIs, load balancing | HTTP infrastructure | Higher latency |
| **WebSocket** | Real-time, bidirectional | Low latency, full-duplex | Requires WS support |

### Recommendations

- **Local tools/servers**: Use `McpStdioTransport` with subprocess
- **Cloud-hosted MCP**: Use `McpHttpTransport` or `McpWebSocketTransport`
- **Real-time updates**: Prefer `McpWebSocketTransport`
- **Behind proxies**: `McpHttpTransport` often works better

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
