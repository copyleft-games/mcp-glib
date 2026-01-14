/*
 * mcp-common.h - Shared utilities for MCP CLI tools
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file provides common functionality shared across all MCP CLI tools:
 * - Command-line option parsing helpers
 * - Transport creation from parsed options
 * - Synchronous client connection
 * - Output formatting for tools, resources, prompts
 * - License display
 */

#ifndef MCP_COMMON_H
#define MCP_COMMON_H

#include <mcp.h>

G_BEGIN_DECLS

/*
 * MCP_CLI_LICENSE:
 *
 * The AGPLv3 license notice displayed by --license option.
 */
#define MCP_CLI_LICENSE \
    "mcp-glib CLI Tools\n" \
    "Copyright (C) 2025 Copyleft Games\n" \
    "\n" \
    "This program is free software: you can redistribute it and/or modify\n" \
    "it under the terms of the GNU Affero General Public License as published\n" \
    "by the Free Software Foundation, either version 3 of the License, or\n" \
    "(at your option) any later version.\n" \
    "\n" \
    "This program is distributed in the hope that it will be useful,\n" \
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n" \
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" \
    "GNU Affero General Public License for more details.\n" \
    "\n" \
    "You should have received a copy of the GNU Affero General Public License\n" \
    "along with this program.  If not, see <https://www.gnu.org/licenses/>.\n"

/*
 * MCP_CLI_BUG_URL:
 *
 * URL for reporting bugs.
 */
#define MCP_CLI_BUG_URL "https://gitlab.com/copyleftgames/mcp-glib/-/issues"

/*
 * Exit codes for CLI tools.
 */
#define MCP_CLI_EXIT_SUCCESS        (0)
#define MCP_CLI_EXIT_ERROR          (1)
#define MCP_CLI_EXIT_NOT_FOUND      (2)
#define MCP_CLI_EXIT_TOOL_ERROR     (3)

/*
 * Common option variables.
 * Declare these as extern in each tool and define in mcp-common.c.
 */
extern gchar    *mcp_cli_opt_stdio;
extern gchar    *mcp_cli_opt_http;
extern gchar    *mcp_cli_opt_ws;
extern gchar    *mcp_cli_opt_token;
extern gint      mcp_cli_opt_timeout;
extern gboolean  mcp_cli_opt_json;
extern gboolean  mcp_cli_opt_quiet;
extern gboolean  mcp_cli_opt_license;

/*
 * mcp_cli_get_common_options:
 *
 * Returns the common GOptionEntry array for transport and output options.
 * The returned array is statically allocated and should not be freed.
 *
 * Returns: (transfer none): array of GOptionEntry terminated by NULL entry
 */
GOptionEntry *mcp_cli_get_common_options (void);

/*
 * mcp_cli_reset_options:
 *
 * Resets all common option variables to their defaults.
 * Call this before parsing options if reusing the option context.
 */
void mcp_cli_reset_options (void);

/*
 * mcp_cli_create_transport:
 * @error: (nullable): return location for a #GError
 *
 * Creates a transport based on the parsed common options.
 * Exactly one of --stdio, --http, or --ws must be specified.
 *
 * Returns: (transfer full) (nullable): a new #McpTransport, or %NULL on error
 */
McpTransport *mcp_cli_create_transport (GError **error);

/*
 * mcp_cli_connect_sync:
 * @client: an #McpClient with transport already set
 * @timeout_seconds: connection timeout in seconds (0 for default)
 * @error: (nullable): return location for a #GError
 *
 * Synchronously connects the client and waits for initialization.
 * This runs a main loop internally until connection completes or fails.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_cli_connect_sync (McpClient  *client,
                               guint       timeout_seconds,
                               GError    **error);

/*
 * mcp_cli_disconnect_sync:
 * @client: an #McpClient
 * @error: (nullable): return location for a #GError
 *
 * Synchronously disconnects the client.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean mcp_cli_disconnect_sync (McpClient  *client,
                                  GError    **error);

/*
 * mcp_cli_print_license:
 *
 * Prints the license information to stdout.
 */
void mcp_cli_print_license (void);

/*
 * mcp_cli_print_server_info:
 * @client: a connected #McpClient
 * @json_mode: whether to output JSON format
 *
 * Prints server implementation info (name, version).
 */
void mcp_cli_print_server_info (McpClient *client,
                                gboolean   json_mode);

/*
 * mcp_cli_print_tool_result:
 * @result: (transfer none): the #McpToolResult to print
 * @json_mode: whether to output JSON format
 *
 * Prints a tool result in human-readable or JSON format.
 */
void mcp_cli_print_tool_result (McpToolResult *result,
                                gboolean       json_mode);

/*
 * mcp_cli_print_resource_contents:
 * @contents: (element-type McpResourceContents): list of contents
 * @json_mode: whether to output JSON format
 *
 * Prints resource contents in human-readable or JSON format.
 */
void mcp_cli_print_resource_contents (GList    *contents,
                                      gboolean  json_mode);

/*
 * mcp_cli_print_prompt_result:
 * @result: (transfer none): the #McpPromptResult to print
 * @json_mode: whether to output JSON format
 *
 * Prints a prompt result in human-readable or JSON format.
 */
void mcp_cli_print_prompt_result (McpPromptResult *result,
                                  gboolean         json_mode);

/*
 * mcp_cli_print_tools:
 * @tools: (element-type McpTool): list of tools
 * @json_mode: whether to output JSON format
 * @verbose: whether to include schema details
 *
 * Prints a list of tools.
 */
void mcp_cli_print_tools (GList    *tools,
                          gboolean  json_mode,
                          gboolean  verbose);

/*
 * mcp_cli_print_resources:
 * @resources: (element-type McpResource): list of resources
 * @json_mode: whether to output JSON format
 *
 * Prints a list of resources.
 */
void mcp_cli_print_resources (GList    *resources,
                              gboolean  json_mode);

/*
 * mcp_cli_print_resource_templates:
 * @templates: (element-type McpResourceTemplate): list of templates
 * @json_mode: whether to output JSON format
 *
 * Prints a list of resource templates.
 */
void mcp_cli_print_resource_templates (GList    *templates,
                                       gboolean  json_mode);

/*
 * mcp_cli_print_prompts:
 * @prompts: (element-type McpPrompt): list of prompts
 * @json_mode: whether to output JSON format
 *
 * Prints a list of prompts.
 */
void mcp_cli_print_prompts (GList    *prompts,
                            gboolean  json_mode);

/*
 * mcp_cli_parse_json_args:
 * @json_str: JSON string to parse
 * @error: (nullable): return location for a #GError
 *
 * Parses a JSON string into a JsonObject for tool arguments.
 *
 * Returns: (transfer full) (nullable): a #JsonObject, or %NULL on error
 */
JsonObject *mcp_cli_parse_json_args (const gchar  *json_str,
                                     GError      **error);

/*
 * mcp_cli_parse_prompt_args:
 * @args: (array zero-terminated=1): array of "key=value" strings
 * @n_args: number of arguments
 *
 * Parses key=value pairs into a hash table for prompt arguments.
 *
 * Returns: (transfer full): a #GHashTable mapping strings to strings
 */
GHashTable *mcp_cli_parse_prompt_args (gchar **args,
                                       gint    n_args);

G_END_DECLS

#endif /* MCP_COMMON_H */
