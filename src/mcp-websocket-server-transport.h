/*
 * mcp-websocket-server-transport.h - WebSocket server transport for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the WebSocket server transport implementation that accepts
 * connections from MCP clients over WebSocket. It provides:
 * - Bidirectional communication via WebSocket text frames
 * - Optional Bearer token authentication
 * - Keepalive via periodic pings
 *
 * This is the server-side counterpart to McpWebSocketTransport (client transport).
 */

#ifndef MCP_WEBSOCKET_SERVER_TRANSPORT_H
#define MCP_WEBSOCKET_SERVER_TRANSPORT_H


#include <glib-object.h>
#include <gio/gio.h>
#include <libsoup/soup.h>
#include "mcp-transport.h"

G_BEGIN_DECLS

#define MCP_TYPE_WEBSOCKET_SERVER_TRANSPORT (mcp_websocket_server_transport_get_type ())

G_DECLARE_FINAL_TYPE (McpWebSocketServerTransport, mcp_websocket_server_transport, MCP, WEBSOCKET_SERVER_TRANSPORT, GObject)

/**
 * mcp_websocket_server_transport_new:
 * @port: the port to listen on (0 = auto-assign)
 *
 * Creates a new WebSocket server transport that listens on all interfaces.
 *
 * Returns: (transfer full): a new #McpWebSocketServerTransport
 */
McpWebSocketServerTransport *mcp_websocket_server_transport_new (guint port);

/**
 * mcp_websocket_server_transport_new_full:
 * @host: (nullable): the host/address to bind to (%NULL = all interfaces)
 * @port: the port to listen on (0 = auto-assign)
 * @path: (nullable): the WebSocket endpoint path (%NULL = "/")
 *
 * Creates a new WebSocket server transport with specific configuration.
 *
 * Returns: (transfer full): a new #McpWebSocketServerTransport
 */
McpWebSocketServerTransport *mcp_websocket_server_transport_new_full (const gchar *host,
                                                                        guint        port,
                                                                        const gchar *path);

/**
 * mcp_websocket_server_transport_get_port:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets the configured port. If port was 0 (auto-assign), this returns
 * the actual assigned port after the transport is connected.
 *
 * Returns: the port number
 */
guint mcp_websocket_server_transport_get_port (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_set_port:
 * @self: an #McpWebSocketServerTransport
 * @port: the port to listen on
 *
 * Sets the port to listen on. Cannot be changed while connected.
 */
void mcp_websocket_server_transport_set_port (McpWebSocketServerTransport *self,
                                               guint                        port);

/**
 * mcp_websocket_server_transport_get_host:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets the host/address binding.
 *
 * Returns: (transfer none) (nullable): the host, or %NULL for all interfaces
 */
const gchar *mcp_websocket_server_transport_get_host (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_set_host:
 * @self: an #McpWebSocketServerTransport
 * @host: (nullable): the host/address to bind to
 *
 * Sets the host/address to bind to. Cannot be changed while connected.
 */
void mcp_websocket_server_transport_set_host (McpWebSocketServerTransport *self,
                                               const gchar                  *host);

/**
 * mcp_websocket_server_transport_get_path:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets the WebSocket endpoint path.
 *
 * Returns: (transfer none): the path
 */
const gchar *mcp_websocket_server_transport_get_path (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_set_path:
 * @self: an #McpWebSocketServerTransport
 * @path: the WebSocket endpoint path
 *
 * Sets the WebSocket endpoint path. Default is "/".
 */
void mcp_websocket_server_transport_set_path (McpWebSocketServerTransport *self,
                                               const gchar                  *path);

/**
 * mcp_websocket_server_transport_get_protocols:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets the accepted WebSocket subprotocols.
 *
 * Returns: (transfer none) (array zero-terminated=1) (nullable): the protocols, or %NULL
 */
const gchar * const *mcp_websocket_server_transport_get_protocols (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_set_protocols:
 * @self: an #McpWebSocketServerTransport
 * @protocols: (array zero-terminated=1) (nullable): the accepted subprotocols
 *
 * Sets the accepted WebSocket subprotocols.
 */
void mcp_websocket_server_transport_set_protocols (McpWebSocketServerTransport *self,
                                                    const gchar * const          *protocols);

/**
 * mcp_websocket_server_transport_get_origin:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets the required Origin header value.
 *
 * Returns: (transfer none) (nullable): the origin, or %NULL to accept any
 */
const gchar *mcp_websocket_server_transport_get_origin (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_set_origin:
 * @self: an #McpWebSocketServerTransport
 * @origin: (nullable): the required Origin header value
 *
 * Sets the required Origin header. If set, clients must send a matching Origin.
 */
void mcp_websocket_server_transport_set_origin (McpWebSocketServerTransport *self,
                                                 const gchar                  *origin);

/**
 * mcp_websocket_server_transport_get_require_auth:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets whether authentication is required.
 *
 * Returns: %TRUE if auth is required
 */
gboolean mcp_websocket_server_transport_get_require_auth (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_set_require_auth:
 * @self: an #McpWebSocketServerTransport
 * @require_auth: whether to require authentication
 *
 * Sets whether clients must provide a valid Bearer token.
 */
void mcp_websocket_server_transport_set_require_auth (McpWebSocketServerTransport *self,
                                                       gboolean                      require_auth);

/**
 * mcp_websocket_server_transport_get_auth_token:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets the expected authentication token.
 *
 * Returns: (transfer none) (nullable): the auth token, or %NULL
 */
const gchar *mcp_websocket_server_transport_get_auth_token (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_set_auth_token:
 * @self: an #McpWebSocketServerTransport
 * @token: (nullable): the expected Bearer token
 *
 * Sets the expected Bearer token for authentication.
 */
void mcp_websocket_server_transport_set_auth_token (McpWebSocketServerTransport *self,
                                                     const gchar                  *token);

/**
 * mcp_websocket_server_transport_get_keepalive_interval:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets the keepalive ping interval.
 *
 * Returns: the interval in seconds (0 = disabled)
 */
guint mcp_websocket_server_transport_get_keepalive_interval (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_set_keepalive_interval:
 * @self: an #McpWebSocketServerTransport
 * @interval_seconds: the keepalive interval in seconds (0 = disabled)
 *
 * Sets the interval for sending ping frames to keep the connection alive.
 */
void mcp_websocket_server_transport_set_keepalive_interval (McpWebSocketServerTransport *self,
                                                             guint                        interval_seconds);

/**
 * mcp_websocket_server_transport_get_tls_certificate:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets the TLS certificate for WSS.
 *
 * Returns: (transfer none) (nullable): the #GTlsCertificate, or %NULL
 */
GTlsCertificate *mcp_websocket_server_transport_get_tls_certificate (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_set_tls_certificate:
 * @self: an #McpWebSocketServerTransport
 * @certificate: (nullable): the TLS certificate for WSS
 *
 * Sets the TLS certificate to enable WSS. Must be set before connecting.
 */
void mcp_websocket_server_transport_set_tls_certificate (McpWebSocketServerTransport *self,
                                                          GTlsCertificate              *certificate);

/**
 * mcp_websocket_server_transport_get_soup_server:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets the underlying SoupServer. Only available after connecting.
 *
 * Returns: (transfer none) (nullable): the #SoupServer, or %NULL
 */
SoupServer *mcp_websocket_server_transport_get_soup_server (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_get_websocket_connection:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets the current WebSocket connection if a client is connected.
 *
 * Returns: (transfer none) (nullable): the #SoupWebsocketConnection, or %NULL
 */
SoupWebsocketConnection *mcp_websocket_server_transport_get_websocket_connection (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_has_client:
 * @self: an #McpWebSocketServerTransport
 *
 * Checks if a client is currently connected.
 *
 * Returns: %TRUE if a client is connected
 */
gboolean mcp_websocket_server_transport_has_client (McpWebSocketServerTransport *self);

/**
 * mcp_websocket_server_transport_get_actual_port:
 * @self: an #McpWebSocketServerTransport
 *
 * Gets the actual port the server is listening on.
 * This is useful when port 0 was specified (auto-assign).
 * Returns 0 if not yet connected.
 *
 * Returns: the actual port number, or 0 if not connected
 */
guint mcp_websocket_server_transport_get_actual_port (McpWebSocketServerTransport *self);

G_END_DECLS

#endif /* MCP_WEBSOCKET_SERVER_TRANSPORT_H */
