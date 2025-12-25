/*
 * mcp-http-transport.h - HTTP transport for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the HTTP transport implementation that communicates
 * via HTTP POST (client to server) and Server-Sent Events (server to client).
 * This follows the MCP Streamable HTTP transport specification.
 *
 * The transport uses:
 * - HTTP POST requests to send JSON-RPC messages
 * - Server-Sent Events (SSE) for receiving responses and notifications
 * - Optional session management via Mcp-Session-Id header
 */

#ifndef MCP_HTTP_TRANSPORT_H
#define MCP_HTTP_TRANSPORT_H

#if !defined(MCP_INSIDE) && !defined(MCP_COMPILATION)
#error "Only <mcp/mcp.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include <libsoup/soup.h>
#include <mcp/mcp-transport.h>

G_BEGIN_DECLS

#define MCP_TYPE_HTTP_TRANSPORT (mcp_http_transport_get_type ())

G_DECLARE_FINAL_TYPE (McpHttpTransport, mcp_http_transport, MCP, HTTP_TRANSPORT, GObject)

/**
 * mcp_http_transport_new:
 * @base_url: the base URL for the MCP server (e.g., "http://localhost:8080/mcp")
 *
 * Creates a new HTTP transport for connecting to an MCP server.
 *
 * Returns: (transfer full): a new #McpHttpTransport
 */
McpHttpTransport *mcp_http_transport_new (const gchar *base_url);

/**
 * mcp_http_transport_new_with_session:
 * @base_url: the base URL for the MCP server
 * @session: (transfer none): a #SoupSession to use for requests
 *
 * Creates a new HTTP transport using a custom SoupSession.
 * This allows sharing a session across multiple transports
 * or configuring custom session settings.
 *
 * Returns: (transfer full): a new #McpHttpTransport
 */
McpHttpTransport *mcp_http_transport_new_with_session (const gchar *base_url,
                                                        SoupSession *session);

/**
 * mcp_http_transport_get_base_url:
 * @self: an #McpHttpTransport
 *
 * Gets the base URL.
 *
 * Returns: (transfer none): the base URL
 */
const gchar *mcp_http_transport_get_base_url (McpHttpTransport *self);

/**
 * mcp_http_transport_set_base_url:
 * @self: an #McpHttpTransport
 * @base_url: the new base URL
 *
 * Sets the base URL. Cannot be changed while connected.
 */
void mcp_http_transport_set_base_url (McpHttpTransport *self,
                                       const gchar      *base_url);

/**
 * mcp_http_transport_get_auth_token:
 * @self: an #McpHttpTransport
 *
 * Gets the authentication token.
 *
 * Returns: (transfer none) (nullable): the auth token, or %NULL
 */
const gchar *mcp_http_transport_get_auth_token (McpHttpTransport *self);

/**
 * mcp_http_transport_set_auth_token:
 * @self: an #McpHttpTransport
 * @token: (nullable): the authentication token (Bearer token)
 *
 * Sets the authentication token for requests.
 * The token will be sent as a Bearer token in the Authorization header.
 */
void mcp_http_transport_set_auth_token (McpHttpTransport *self,
                                         const gchar      *token);

/**
 * mcp_http_transport_get_session_id:
 * @self: an #McpHttpTransport
 *
 * Gets the MCP session ID assigned by the server.
 * This is only available after a successful connection.
 *
 * Returns: (transfer none) (nullable): the session ID, or %NULL
 */
const gchar *mcp_http_transport_get_session_id (McpHttpTransport *self);

/**
 * mcp_http_transport_get_soup_session:
 * @self: an #McpHttpTransport
 *
 * Gets the underlying SoupSession.
 *
 * Returns: (transfer none): the #SoupSession
 */
SoupSession *mcp_http_transport_get_soup_session (McpHttpTransport *self);

/**
 * mcp_http_transport_set_timeout:
 * @self: an #McpHttpTransport
 * @timeout_seconds: timeout in seconds (0 = no timeout)
 *
 * Sets the request timeout for HTTP requests.
 */
void mcp_http_transport_set_timeout (McpHttpTransport *self,
                                      guint             timeout_seconds);

/**
 * mcp_http_transport_get_timeout:
 * @self: an #McpHttpTransport
 *
 * Gets the request timeout.
 *
 * Returns: the timeout in seconds
 */
guint mcp_http_transport_get_timeout (McpHttpTransport *self);

/**
 * mcp_http_transport_set_sse_endpoint:
 * @self: an #McpHttpTransport
 * @endpoint: (nullable): the SSE endpoint path (relative to base URL)
 *
 * Sets the SSE endpoint for receiving server messages.
 * If %NULL, the transport uses only the base URL with Accept: text/event-stream.
 * Default is %NULL (use base URL).
 */
void mcp_http_transport_set_sse_endpoint (McpHttpTransport *self,
                                           const gchar      *endpoint);

/**
 * mcp_http_transport_get_sse_endpoint:
 * @self: an #McpHttpTransport
 *
 * Gets the SSE endpoint.
 *
 * Returns: (transfer none) (nullable): the SSE endpoint, or %NULL
 */
const gchar *mcp_http_transport_get_sse_endpoint (McpHttpTransport *self);

/**
 * mcp_http_transport_set_post_endpoint:
 * @self: an #McpHttpTransport
 * @endpoint: (nullable): the POST endpoint path (relative to base URL)
 *
 * Sets the POST endpoint for sending messages.
 * If %NULL, the transport posts to the base URL.
 * Default is %NULL (use base URL).
 */
void mcp_http_transport_set_post_endpoint (McpHttpTransport *self,
                                            const gchar      *endpoint);

/**
 * mcp_http_transport_get_post_endpoint:
 * @self: an #McpHttpTransport
 *
 * Gets the POST endpoint.
 *
 * Returns: (transfer none) (nullable): the POST endpoint, or %NULL
 */
const gchar *mcp_http_transport_get_post_endpoint (McpHttpTransport *self);

/**
 * mcp_http_transport_set_reconnect_enabled:
 * @self: an #McpHttpTransport
 * @enabled: whether to enable automatic reconnection
 *
 * Enables or disables automatic reconnection on SSE disconnect.
 */
void mcp_http_transport_set_reconnect_enabled (McpHttpTransport *self,
                                                gboolean          enabled);

/**
 * mcp_http_transport_get_reconnect_enabled:
 * @self: an #McpHttpTransport
 *
 * Gets whether automatic reconnection is enabled.
 *
 * Returns: %TRUE if enabled
 */
gboolean mcp_http_transport_get_reconnect_enabled (McpHttpTransport *self);

/**
 * mcp_http_transport_set_reconnect_delay:
 * @self: an #McpHttpTransport
 * @delay_ms: the reconnect delay in milliseconds
 *
 * Sets the delay before attempting reconnection.
 */
void mcp_http_transport_set_reconnect_delay (McpHttpTransport *self,
                                              guint             delay_ms);

/**
 * mcp_http_transport_get_reconnect_delay:
 * @self: an #McpHttpTransport
 *
 * Gets the reconnect delay.
 *
 * Returns: the delay in milliseconds
 */
guint mcp_http_transport_get_reconnect_delay (McpHttpTransport *self);

G_END_DECLS

#endif /* MCP_HTTP_TRANSPORT_H */
