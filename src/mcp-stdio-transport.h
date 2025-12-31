/*
 * mcp-stdio-transport.h - Stdio transport for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the stdio transport implementation that communicates
 * via standard input/output streams. This is the default transport for
 * MCP servers launched as subprocesses.
 *
 * Message framing uses newline-delimited JSON (NDJSON):
 * - Each JSON message is terminated by a newline character
 * - Messages are parsed one line at a time
 */

#ifndef MCP_STDIO_TRANSPORT_H
#define MCP_STDIO_TRANSPORT_H


#include <glib-object.h>
#include <gio/gio.h>
#include "mcp-transport.h"

G_BEGIN_DECLS

#define MCP_TYPE_STDIO_TRANSPORT (mcp_stdio_transport_get_type ())

G_DECLARE_FINAL_TYPE (McpStdioTransport, mcp_stdio_transport, MCP, STDIO_TRANSPORT, GObject)

/**
 * mcp_stdio_transport_new:
 *
 * Creates a new stdio transport that uses the process's stdin and stdout.
 * This is typically used by MCP servers.
 *
 * Returns: (transfer full): a new #McpStdioTransport
 */
McpStdioTransport *mcp_stdio_transport_new (void);

/**
 * mcp_stdio_transport_new_with_streams:
 * @input: (transfer none): the input stream to read from
 * @output: (transfer none): the output stream to write to
 *
 * Creates a new stdio transport with custom streams.
 * This is useful for testing or for wrapping other I/O sources.
 *
 * Returns: (transfer full): a new #McpStdioTransport
 */
McpStdioTransport *mcp_stdio_transport_new_with_streams (GInputStream  *input,
                                                          GOutputStream *output);

/**
 * mcp_stdio_transport_new_subprocess:
 * @command: (array zero-terminated=1): the command to execute
 * @error: (nullable): return location for a #GError
 *
 * Creates a new stdio transport that spawns a subprocess.
 * The transport communicates with the subprocess via its stdin/stdout.
 * This is typically used by MCP clients.
 *
 * Returns: (transfer full) (nullable): a new #McpStdioTransport, or %NULL on error
 */
McpStdioTransport *mcp_stdio_transport_new_subprocess (const gchar * const *command,
                                                        GError             **error);

/**
 * mcp_stdio_transport_new_subprocess_simple:
 * @command_line: the command line to execute (parsed by shell rules)
 * @error: (nullable): return location for a #GError
 *
 * Creates a new stdio transport that spawns a subprocess using shell parsing.
 * The command line is parsed using shell rules (quotes, escaping, etc.).
 *
 * Returns: (transfer full) (nullable): a new #McpStdioTransport, or %NULL on error
 */
McpStdioTransport *mcp_stdio_transport_new_subprocess_simple (const gchar  *command_line,
                                                               GError      **error);

/**
 * mcp_stdio_transport_get_subprocess:
 * @self: an #McpStdioTransport
 *
 * Gets the subprocess if this transport was created with
 * mcp_stdio_transport_new_subprocess().
 *
 * Returns: (transfer none) (nullable): the #GSubprocess, or %NULL
 */
GSubprocess *mcp_stdio_transport_get_subprocess (McpStdioTransport *self);

/**
 * mcp_stdio_transport_get_input_stream:
 * @self: an #McpStdioTransport
 *
 * Gets the input stream.
 *
 * Returns: (transfer none): the #GInputStream
 */
GInputStream *mcp_stdio_transport_get_input_stream (McpStdioTransport *self);

/**
 * mcp_stdio_transport_get_output_stream:
 * @self: an #McpStdioTransport
 *
 * Gets the output stream.
 *
 * Returns: (transfer none): the #GOutputStream
 */
GOutputStream *mcp_stdio_transport_get_output_stream (McpStdioTransport *self);

G_END_DECLS

#endif /* MCP_STDIO_TRANSPORT_H */
