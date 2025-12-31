/*
 * mcp.h - Main umbrella header for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This is the main include file for mcp-glib. Include this header
 * to get access to all public APIs.
 *
 * Example:
 * |[<!-- language="C" -->
 * #include <mcp/mcp.h>
 *
 * int
 * main (int argc, char **argv)
 * {
 *     g_autoptr(McpServer) server = NULL;
 *
 *     server = mcp_server_new ("my-server", "1.0.0");
 *     // ... add tools, resources, prompts ...
 *     return 0;
 * }
 * ]|
 */

#ifndef MCP_H
#define MCP_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/* Version information */
#include "mcp-version.h"

/* Enumerations */
#include "mcp-enums.h"

/* Error handling */
#include "mcp-error.h"

/* Entity types */
#include "mcp-tool.h"
#include "mcp-resource.h"
#include "mcp-prompt.h"
#include "mcp-task.h"

/* Sampling types */
#include "mcp-sampling.h"

/* Root type */
#include "mcp-root.h"

/* Completion types */
#include "mcp-completion.h"

/* Message types */
#include "mcp-message.h"

/* Capabilities and Session */
#include "mcp-capabilities.h"
#include "mcp-session.h"

/* Transport */
#include "mcp-transport.h"

/*
 * Stdio transport requires platform-specific stream APIs.
 * On Windows cross-compilation via mingw, gwin32inputstream.h is unavailable.
 * Define MCP_NO_STDIO_TRANSPORT to exclude this transport.
 */
#ifndef MCP_NO_STDIO_TRANSPORT
#include "mcp-stdio-transport.h"
#endif

/*
 * HTTP and WebSocket transports require libsoup-3.0 which may not be
 * available on all platforms (e.g., Windows via mingw).
 * Define MCP_NO_LIBSOUP to exclude these transports.
 */
#ifndef MCP_NO_LIBSOUP
#include "mcp-http-transport.h"
#include "mcp-websocket-transport.h"
#endif

/* Provider interfaces */
#include "mcp-tool-provider.h"
#include "mcp-resource-provider.h"
#include "mcp-prompt-provider.h"

/* Server and Client */
#include "mcp-server.h"
#include "mcp-client.h"

G_END_DECLS

#endif /* MCP_H */
