/*
 * mcp-http-server-transport.h - HTTP server transport for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the HTTP server transport implementation that accepts
 * connections from MCP clients over HTTP. It uses:
 * - HTTP POST requests to receive JSON-RPC messages from clients
 * - Server-Sent Events (SSE) to send responses and notifications to clients
 * - Optional session management via Mcp-Session-Id header
 * - Optional Bearer token authentication
 *
 * This is the server-side counterpart to McpHttpTransport (client transport).
 */

#ifndef MCP_HTTP_SERVER_TRANSPORT_H
#define MCP_HTTP_SERVER_TRANSPORT_H


#include <glib-object.h>
#include <gio/gio.h>
#include <libsoup/soup.h>
#include "mcp-transport.h"

G_BEGIN_DECLS

#define MCP_TYPE_HTTP_SERVER_TRANSPORT (mcp_http_server_transport_get_type ())

G_DECLARE_FINAL_TYPE (McpHttpServerTransport, mcp_http_server_transport, MCP, HTTP_SERVER_TRANSPORT, GObject)

/**
 * mcp_http_server_transport_new:
 * @port: the port to listen on (0 = auto-assign)
 *
 * Creates a new HTTP server transport that listens on all interfaces.
 *
 * Returns: (transfer full): a new #McpHttpServerTransport
 */
McpHttpServerTransport *mcp_http_server_transport_new (guint port);

/**
 * mcp_http_server_transport_new_full:
 * @host: (nullable): the host/address to bind to (%NULL = all interfaces)
 * @port: the port to listen on (0 = auto-assign)
 *
 * Creates a new HTTP server transport with specific host binding.
 *
 * Returns: (transfer full): a new #McpHttpServerTransport
 */
McpHttpServerTransport *mcp_http_server_transport_new_full (const gchar *host,
                                                             guint        port);

/**
 * mcp_http_server_transport_get_port:
 * @self: an #McpHttpServerTransport
 *
 * Gets the configured port. If port was 0 (auto-assign), this returns
 * the actual assigned port after the transport is connected.
 *
 * Returns: the port number
 */
guint mcp_http_server_transport_get_port (McpHttpServerTransport *self);

/**
 * mcp_http_server_transport_set_port:
 * @self: an #McpHttpServerTransport
 * @port: the port to listen on
 *
 * Sets the port to listen on. Cannot be changed while connected.
 */
void mcp_http_server_transport_set_port (McpHttpServerTransport *self,
                                          guint                   port);

/**
 * mcp_http_server_transport_get_host:
 * @self: an #McpHttpServerTransport
 *
 * Gets the host/address binding.
 *
 * Returns: (transfer none) (nullable): the host, or %NULL for all interfaces
 */
const gchar *mcp_http_server_transport_get_host (McpHttpServerTransport *self);

/**
 * mcp_http_server_transport_set_host:
 * @self: an #McpHttpServerTransport
 * @host: (nullable): the host/address to bind to
 *
 * Sets the host/address to bind to. Cannot be changed while connected.
 */
void mcp_http_server_transport_set_host (McpHttpServerTransport *self,
                                          const gchar            *host);

/**
 * mcp_http_server_transport_get_post_path:
 * @self: an #McpHttpServerTransport
 *
 * Gets the path for POST requests.
 *
 * Returns: (transfer none): the POST path
 */
const gchar *mcp_http_server_transport_get_post_path (McpHttpServerTransport *self);

/**
 * mcp_http_server_transport_set_post_path:
 * @self: an #McpHttpServerTransport
 * @path: the path for POST requests
 *
 * Sets the path where clients send POST requests.
 * Default is "/".
 */
void mcp_http_server_transport_set_post_path (McpHttpServerTransport *self,
                                               const gchar            *path);

/**
 * mcp_http_server_transport_get_sse_path:
 * @self: an #McpHttpServerTransport
 *
 * Gets the path for SSE connections.
 *
 * Returns: (transfer none): the SSE path
 */
const gchar *mcp_http_server_transport_get_sse_path (McpHttpServerTransport *self);

/**
 * mcp_http_server_transport_set_sse_path:
 * @self: an #McpHttpServerTransport
 * @path: the path for SSE connections
 *
 * Sets the path where clients connect for SSE events.
 * Default is "/sse".
 */
void mcp_http_server_transport_set_sse_path (McpHttpServerTransport *self,
                                              const gchar            *path);

/**
 * mcp_http_server_transport_get_session_id:
 * @self: an #McpHttpServerTransport
 *
 * Gets the current session ID. This is generated when a client
 * connects via SSE and is used to validate POST requests.
 *
 * Returns: (transfer none) (nullable): the session ID, or %NULL if no client connected
 */
const gchar *mcp_http_server_transport_get_session_id (McpHttpServerTransport *self);

/**
 * mcp_http_server_transport_get_require_auth:
 * @self: an #McpHttpServerTransport
 *
 * Gets whether authentication is required.
 *
 * Returns: %TRUE if auth is required
 */
gboolean mcp_http_server_transport_get_require_auth (McpHttpServerTransport *self);

/**
 * mcp_http_server_transport_set_require_auth:
 * @self: an #McpHttpServerTransport
 * @require_auth: whether to require authentication
 *
 * Sets whether clients must provide a valid Bearer token.
 */
void mcp_http_server_transport_set_require_auth (McpHttpServerTransport *self,
                                                  gboolean                require_auth);

/**
 * mcp_http_server_transport_get_auth_token:
 * @self: an #McpHttpServerTransport
 *
 * Gets the expected authentication token.
 *
 * Returns: (transfer none) (nullable): the auth token, or %NULL
 */
const gchar *mcp_http_server_transport_get_auth_token (McpHttpServerTransport *self);

/**
 * mcp_http_server_transport_set_auth_token:
 * @self: an #McpHttpServerTransport
 * @token: (nullable): the expected Bearer token
 *
 * Sets the expected Bearer token for authentication.
 * Clients must provide this token in the Authorization header
 * when require-auth is enabled.
 */
void mcp_http_server_transport_set_auth_token (McpHttpServerTransport *self,
                                                const gchar            *token);

/**
 * mcp_http_server_transport_get_tls_certificate:
 * @self: an #McpHttpServerTransport
 *
 * Gets the TLS certificate for HTTPS.
 *
 * Returns: (transfer none) (nullable): the #GTlsCertificate, or %NULL
 */
GTlsCertificate *mcp_http_server_transport_get_tls_certificate (McpHttpServerTransport *self);

/**
 * mcp_http_server_transport_set_tls_certificate:
 * @self: an #McpHttpServerTransport
 * @certificate: (nullable): the TLS certificate for HTTPS
 *
 * Sets the TLS certificate to enable HTTPS. Must be set before connecting.
 */
void mcp_http_server_transport_set_tls_certificate (McpHttpServerTransport *self,
                                                     GTlsCertificate        *certificate);

/**
 * mcp_http_server_transport_get_soup_server:
 * @self: an #McpHttpServerTransport
 *
 * Gets the underlying SoupServer. This is only available after
 * the transport has been connected.
 *
 * Returns: (transfer none) (nullable): the #SoupServer, or %NULL
 */
SoupServer *mcp_http_server_transport_get_soup_server (McpHttpServerTransport *self);

/**
 * mcp_http_server_transport_has_client:
 * @self: an #McpHttpServerTransport
 *
 * Checks if a client is currently connected via SSE.
 *
 * Returns: %TRUE if a client is connected
 */
gboolean mcp_http_server_transport_has_client (McpHttpServerTransport *self);

/**
 * mcp_http_server_transport_get_actual_port:
 * @self: an #McpHttpServerTransport
 *
 * Gets the actual port the server is listening on.
 * This is useful when port 0 was specified (auto-assign).
 * Returns 0 if not yet connected.
 *
 * Returns: the actual port number, or 0 if not connected
 */
guint mcp_http_server_transport_get_actual_port (McpHttpServerTransport *self);

G_END_DECLS

#endif /* MCP_HTTP_SERVER_TRANSPORT_H */
