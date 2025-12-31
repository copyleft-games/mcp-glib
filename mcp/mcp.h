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

#define MCP_INSIDE

/* Version information */
#include <mcp/mcp-version.h>

/* Enumerations */
#include <mcp/mcp-enums.h>

/* Error handling */
#include <mcp/mcp-error.h>

/* Entity types */
#include <mcp/mcp-tool.h>
#include <mcp/mcp-resource.h>
#include <mcp/mcp-prompt.h>
#include <mcp/mcp-task.h>

/* Sampling types */
#include <mcp/mcp-sampling.h>

/* Root type */
#include <mcp/mcp-root.h>

/* Completion types */
#include <mcp/mcp-completion.h>

/* Message types */
#include <mcp/mcp-message.h>

/* Capabilities and Session */
#include <mcp/mcp-capabilities.h>
#include <mcp/mcp-session.h>

/* Transport */
#include <mcp/mcp-transport.h>
#include <mcp/mcp-stdio-transport.h>
#include <mcp/mcp-http-transport.h>
#include <mcp/mcp-websocket-transport.h>

/* Provider interfaces */
#include <mcp/mcp-tool-provider.h>
#include <mcp/mcp-resource-provider.h>
#include <mcp/mcp-prompt-provider.h>

/* Server and Client */
#include <mcp/mcp-server.h>
#include <mcp/mcp-client.h>

#undef MCP_INSIDE

G_END_DECLS

#endif /* MCP_H */
