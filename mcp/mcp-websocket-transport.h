/*
 * mcp-websocket-transport.h - WebSocket transport for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the WebSocket transport implementation that provides
 * bidirectional communication between MCP clients and servers.
 *
 * The transport uses:
 * - WebSocket text frames for JSON-RPC messages
 * - Automatic ping/pong for connection keepalive
 * - Optional reconnection on disconnect
 */

#ifndef MCP_WEBSOCKET_TRANSPORT_H
#define MCP_WEBSOCKET_TRANSPORT_H

#if !defined(MCP_INSIDE) && !defined(MCP_COMPILATION)
#error "Only <mcp/mcp.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include <libsoup/soup.h>
#include <mcp/mcp-transport.h>

G_BEGIN_DECLS

#define MCP_TYPE_WEBSOCKET_TRANSPORT (mcp_websocket_transport_get_type ())

G_DECLARE_FINAL_TYPE (McpWebSocketTransport, mcp_websocket_transport, MCP, WEBSOCKET_TRANSPORT, GObject)

/**
 * mcp_websocket_transport_new:
 * @uri: the WebSocket URI (e.g., "ws://localhost:8080/mcp" or "wss://...")
 *
 * Creates a new WebSocket transport for connecting to an MCP server.
 *
 * Returns: (transfer full): a new #McpWebSocketTransport
 */
McpWebSocketTransport *mcp_websocket_transport_new (const gchar *uri);

/**
 * mcp_websocket_transport_new_with_session:
 * @uri: the WebSocket URI
 * @session: (transfer none): a #SoupSession to use for the connection
 *
 * Creates a new WebSocket transport using a custom SoupSession.
 *
 * Returns: (transfer full): a new #McpWebSocketTransport
 */
McpWebSocketTransport *mcp_websocket_transport_new_with_session (const gchar *uri,
                                                                   SoupSession *session);

/**
 * mcp_websocket_transport_get_uri:
 * @self: an #McpWebSocketTransport
 *
 * Gets the WebSocket URI.
 *
 * Returns: (transfer none): the URI
 */
const gchar *mcp_websocket_transport_get_uri (McpWebSocketTransport *self);

/**
 * mcp_websocket_transport_set_uri:
 * @self: an #McpWebSocketTransport
 * @uri: the new WebSocket URI
 *
 * Sets the WebSocket URI. Cannot be changed while connected.
 */
void mcp_websocket_transport_set_uri (McpWebSocketTransport *self,
                                       const gchar           *uri);

/**
 * mcp_websocket_transport_get_auth_token:
 * @self: an #McpWebSocketTransport
 *
 * Gets the authentication token.
 *
 * Returns: (transfer none) (nullable): the auth token, or %NULL
 */
const gchar *mcp_websocket_transport_get_auth_token (McpWebSocketTransport *self);

/**
 * mcp_websocket_transport_set_auth_token:
 * @self: an #McpWebSocketTransport
 * @token: (nullable): the authentication token (Bearer token)
 *
 * Sets the authentication token for the connection.
 * The token will be sent as a Bearer token in the Authorization header
 * during the WebSocket handshake.
 */
void mcp_websocket_transport_set_auth_token (McpWebSocketTransport *self,
                                              const gchar           *token);

/**
 * mcp_websocket_transport_get_protocols:
 * @self: an #McpWebSocketTransport
 *
 * Gets the WebSocket subprotocols.
 *
 * Returns: (transfer none) (array zero-terminated=1) (nullable): the protocols, or %NULL
 */
const gchar * const *mcp_websocket_transport_get_protocols (McpWebSocketTransport *self);

/**
 * mcp_websocket_transport_set_protocols:
 * @self: an #McpWebSocketTransport
 * @protocols: (array zero-terminated=1) (nullable): the WebSocket subprotocols
 *
 * Sets the WebSocket subprotocols to request during handshake.
 */
void mcp_websocket_transport_set_protocols (McpWebSocketTransport  *self,
                                             const gchar * const    *protocols);

/**
 * mcp_websocket_transport_get_origin:
 * @self: an #McpWebSocketTransport
 *
 * Gets the Origin header value.
 *
 * Returns: (transfer none) (nullable): the origin, or %NULL
 */
const gchar *mcp_websocket_transport_get_origin (McpWebSocketTransport *self);

/**
 * mcp_websocket_transport_set_origin:
 * @self: an #McpWebSocketTransport
 * @origin: (nullable): the Origin header value
 *
 * Sets the Origin header for the WebSocket handshake.
 */
void mcp_websocket_transport_set_origin (McpWebSocketTransport *self,
                                          const gchar           *origin);

/**
 * mcp_websocket_transport_get_soup_session:
 * @self: an #McpWebSocketTransport
 *
 * Gets the underlying SoupSession.
 *
 * Returns: (transfer none): the #SoupSession
 */
SoupSession *mcp_websocket_transport_get_soup_session (McpWebSocketTransport *self);

/**
 * mcp_websocket_transport_get_websocket_connection:
 * @self: an #McpWebSocketTransport
 *
 * Gets the WebSocket connection if connected.
 *
 * Returns: (transfer none) (nullable): the #SoupWebsocketConnection, or %NULL
 */
SoupWebsocketConnection *mcp_websocket_transport_get_websocket_connection (McpWebSocketTransport *self);

/**
 * mcp_websocket_transport_set_reconnect_enabled:
 * @self: an #McpWebSocketTransport
 * @enabled: whether to enable automatic reconnection
 *
 * Enables or disables automatic reconnection on disconnect.
 */
void mcp_websocket_transport_set_reconnect_enabled (McpWebSocketTransport *self,
                                                     gboolean               enabled);

/**
 * mcp_websocket_transport_get_reconnect_enabled:
 * @self: an #McpWebSocketTransport
 *
 * Gets whether automatic reconnection is enabled.
 *
 * Returns: %TRUE if enabled
 */
gboolean mcp_websocket_transport_get_reconnect_enabled (McpWebSocketTransport *self);

/**
 * mcp_websocket_transport_set_reconnect_delay:
 * @self: an #McpWebSocketTransport
 * @delay_ms: the reconnect delay in milliseconds
 *
 * Sets the delay before attempting reconnection.
 */
void mcp_websocket_transport_set_reconnect_delay (McpWebSocketTransport *self,
                                                   guint                  delay_ms);

/**
 * mcp_websocket_transport_get_reconnect_delay:
 * @self: an #McpWebSocketTransport
 *
 * Gets the reconnect delay.
 *
 * Returns: the delay in milliseconds
 */
guint mcp_websocket_transport_get_reconnect_delay (McpWebSocketTransport *self);

/**
 * mcp_websocket_transport_set_max_reconnect_attempts:
 * @self: an #McpWebSocketTransport
 * @max_attempts: maximum reconnection attempts (0 = unlimited)
 *
 * Sets the maximum number of reconnection attempts.
 */
void mcp_websocket_transport_set_max_reconnect_attempts (McpWebSocketTransport *self,
                                                          guint                  max_attempts);

/**
 * mcp_websocket_transport_get_max_reconnect_attempts:
 * @self: an #McpWebSocketTransport
 *
 * Gets the maximum reconnection attempts.
 *
 * Returns: the maximum attempts (0 = unlimited)
 */
guint mcp_websocket_transport_get_max_reconnect_attempts (McpWebSocketTransport *self);

/**
 * mcp_websocket_transport_set_keepalive_interval:
 * @self: an #McpWebSocketTransport
 * @interval_seconds: the keepalive interval in seconds (0 = disabled)
 *
 * Sets the interval for sending ping frames to keep the connection alive.
 */
void mcp_websocket_transport_set_keepalive_interval (McpWebSocketTransport *self,
                                                      guint                  interval_seconds);

/**
 * mcp_websocket_transport_get_keepalive_interval:
 * @self: an #McpWebSocketTransport
 *
 * Gets the keepalive interval.
 *
 * Returns: the interval in seconds
 */
guint mcp_websocket_transport_get_keepalive_interval (McpWebSocketTransport *self);

G_END_DECLS

#endif /* MCP_WEBSOCKET_TRANSPORT_H */
