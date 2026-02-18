/*
 * mcp-unix-socket-server.h - Unix domain socket MCP server for mcp-glib
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines a multi-client MCP server that listens on a Unix
 * domain socket. Each incoming connection gets its own McpServer and
 * McpStdioTransport pair. The consumer registers tools/resources/prompts
 * by connecting to the "session-created" signal.
 *
 * Unlike the transport classes (McpStdioTransport, McpHttpServerTransport),
 * this class does NOT implement McpTransport. It is a higher-level
 * abstraction that manages multiple 1:1 McpServer:McpTransport pairs
 * internally.
 */

#ifndef MCP_UNIX_SOCKET_SERVER_H
#define MCP_UNIX_SOCKET_SERVER_H


#include <glib-object.h>
#include <gio/gio.h>
#include "mcp-server.h"

G_BEGIN_DECLS

#define MCP_TYPE_UNIX_SOCKET_SERVER (mcp_unix_socket_server_get_type ())

G_DECLARE_FINAL_TYPE (McpUnixSocketServer, mcp_unix_socket_server,
                      MCP, UNIX_SOCKET_SERVER, GObject)

/**
 * mcp_unix_socket_server_new:
 * @server_name: the name passed to each per-connection #McpServer
 * @server_version: the version passed to each per-connection #McpServer
 * @socket_path: the absolute path for the Unix domain socket
 *
 * Creates a new Unix socket MCP server. Call
 * mcp_unix_socket_server_start() to begin accepting connections.
 *
 * Connect to the #McpUnixSocketServer::session-created signal before
 * calling start() to register tools on each new #McpServer.
 *
 * Returns: (transfer full): a new #McpUnixSocketServer
 */
McpUnixSocketServer *mcp_unix_socket_server_new (const gchar *server_name,
                                                  const gchar *server_version,
                                                  const gchar *socket_path);

/**
 * mcp_unix_socket_server_start:
 * @self: an #McpUnixSocketServer
 * @error: (nullable): return location for a #GError
 *
 * Starts listening on the Unix domain socket. Any stale socket file
 * at the configured path is removed before binding.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_unix_socket_server_start (McpUnixSocketServer  *self,
                                        GError              **error);

/**
 * mcp_unix_socket_server_stop:
 * @self: an #McpUnixSocketServer
 *
 * Stops the server: closes all active sessions (emitting
 * #McpUnixSocketServer::session-closed for each), stops the socket
 * listener, and removes the socket file.
 */
void mcp_unix_socket_server_stop (McpUnixSocketServer *self);

/**
 * mcp_unix_socket_server_get_socket_path:
 * @self: an #McpUnixSocketServer
 *
 * Gets the socket path.
 *
 * Returns: (transfer none): the socket path
 */
const gchar *mcp_unix_socket_server_get_socket_path (McpUnixSocketServer *self);

/**
 * mcp_unix_socket_server_get_server_name:
 * @self: an #McpUnixSocketServer
 *
 * Gets the server name used for per-connection #McpServer instances.
 *
 * Returns: (transfer none): the server name
 */
const gchar *mcp_unix_socket_server_get_server_name (McpUnixSocketServer *self);

/**
 * mcp_unix_socket_server_get_server_version:
 * @self: an #McpUnixSocketServer
 *
 * Gets the server version used for per-connection #McpServer instances.
 *
 * Returns: (transfer none): the server version
 */
const gchar *mcp_unix_socket_server_get_server_version (McpUnixSocketServer *self);

/**
 * mcp_unix_socket_server_get_session_count:
 * @self: an #McpUnixSocketServer
 *
 * Gets the number of active client sessions.
 *
 * Returns: the number of active sessions
 */
guint mcp_unix_socket_server_get_session_count (McpUnixSocketServer *self);

/**
 * mcp_unix_socket_server_is_running:
 * @self: an #McpUnixSocketServer
 *
 * Checks whether the server is currently listening for connections.
 *
 * Returns: %TRUE if the server is running
 */
gboolean mcp_unix_socket_server_is_running (McpUnixSocketServer *self);

/**
 * mcp_unix_socket_server_set_instructions:
 * @self: an #McpUnixSocketServer
 * @instructions: (nullable): the instructions text
 *
 * Sets instructions that are automatically applied to every new
 * per-connection #McpServer via mcp_server_set_instructions().
 * Takes effect on the next connection; does not affect existing sessions.
 */
void mcp_unix_socket_server_set_instructions (McpUnixSocketServer *self,
                                               const gchar         *instructions);

/**
 * mcp_unix_socket_server_get_instructions:
 * @self: an #McpUnixSocketServer
 *
 * Gets the instructions text.
 *
 * Returns: (transfer none) (nullable): the instructions, or %NULL
 */
const gchar *mcp_unix_socket_server_get_instructions (McpUnixSocketServer *self);

G_END_DECLS

#endif /* MCP_UNIX_SOCKET_SERVER_H */
