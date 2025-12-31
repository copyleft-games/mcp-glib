/*
 * mcp-version.h - Version information for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef MCP_VERSION_H
#define MCP_VERSION_H


/**
 * MCP_MAJOR_VERSION:
 *
 * The major version component of the mcp-glib library.
 */
#define MCP_MAJOR_VERSION (1)

/**
 * MCP_MINOR_VERSION:
 *
 * The minor version component of the mcp-glib library.
 */
#define MCP_MINOR_VERSION (0)

/**
 * MCP_MICRO_VERSION:
 *
 * The micro version component of the mcp-glib library.
 */
#define MCP_MICRO_VERSION (0)

/**
 * MCP_VERSION_STRING:
 *
 * The version of the mcp-glib library as a string.
 */
#define MCP_VERSION_STRING "1.0.0"

/**
 * MCP_PROTOCOL_VERSION:
 *
 * The latest MCP protocol version supported by this library.
 */
#define MCP_PROTOCOL_VERSION "2025-11-25"

/**
 * MCP_DEFAULT_NEGOTIATED_VERSION:
 *
 * The default negotiated protocol version when none is specified.
 */
#define MCP_DEFAULT_NEGOTIATED_VERSION "2025-03-26"

/**
 * MCP_CHECK_VERSION:
 * @major: the major version to check for
 * @minor: the minor version to check for
 * @micro: the micro version to check for
 *
 * Checks whether the mcp-glib library is at least the specified version.
 *
 * Returns: %TRUE if the version is at least the specified version
 */
#define MCP_CHECK_VERSION(major, minor, micro) \
    (MCP_MAJOR_VERSION > (major) || \
     (MCP_MAJOR_VERSION == (major) && MCP_MINOR_VERSION > (minor)) || \
     (MCP_MAJOR_VERSION == (major) && MCP_MINOR_VERSION == (minor) && MCP_MICRO_VERSION >= (micro)))

#endif /* MCP_VERSION_H */
