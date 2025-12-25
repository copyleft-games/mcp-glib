/*
 * mcp-capabilities.h - Capability negotiation types for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the capability structures used during MCP initialization:
 * - McpImplementation: Server/client identity information
 * - McpServerCapabilities: What the server can do
 * - McpClientCapabilities: What the client can do
 */

#ifndef MCP_CAPABILITIES_H
#define MCP_CAPABILITIES_H

#if !defined(MCP_INSIDE) && !defined(MCP_COMPILATION)
#error "Only <mcp/mcp.h> can be included directly."
#endif

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/* ========================================================================== */
/* McpImplementation - Server/client identity                                 */
/* ========================================================================== */

#define MCP_TYPE_IMPLEMENTATION (mcp_implementation_get_type ())

G_DECLARE_FINAL_TYPE (McpImplementation, mcp_implementation, MCP, IMPLEMENTATION, GObject)

/**
 * mcp_implementation_new:
 * @name: the implementation name (e.g., "my-mcp-server")
 * @version: the implementation version (e.g., "1.0.0")
 *
 * Creates a new implementation info object.
 *
 * Returns: (transfer full): a new #McpImplementation
 */
McpImplementation *mcp_implementation_new (const gchar *name,
                                           const gchar *version);

/**
 * mcp_implementation_get_name:
 * @self: an #McpImplementation
 *
 * Gets the implementation name.
 *
 * Returns: (transfer none): the name
 */
const gchar *mcp_implementation_get_name (McpImplementation *self);

/**
 * mcp_implementation_get_version:
 * @self: an #McpImplementation
 *
 * Gets the implementation version.
 *
 * Returns: (transfer none): the version
 */
const gchar *mcp_implementation_get_version (McpImplementation *self);

/**
 * mcp_implementation_get_title:
 * @self: an #McpImplementation
 *
 * Gets the human-readable title.
 *
 * Returns: (transfer none) (nullable): the title, or %NULL
 */
const gchar *mcp_implementation_get_title (McpImplementation *self);

/**
 * mcp_implementation_set_title:
 * @self: an #McpImplementation
 * @title: (nullable): the title
 *
 * Sets the human-readable title.
 */
void mcp_implementation_set_title (McpImplementation *self,
                                   const gchar       *title);

/**
 * mcp_implementation_get_website_url:
 * @self: an #McpImplementation
 *
 * Gets the website URL.
 *
 * Returns: (transfer none) (nullable): the website URL, or %NULL
 */
const gchar *mcp_implementation_get_website_url (McpImplementation *self);

/**
 * mcp_implementation_set_website_url:
 * @self: an #McpImplementation
 * @url: (nullable): the website URL
 *
 * Sets the website URL.
 */
void mcp_implementation_set_website_url (McpImplementation *self,
                                         const gchar       *url);

/**
 * mcp_implementation_to_json:
 * @self: an #McpImplementation
 *
 * Serializes the implementation info to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_implementation_to_json (McpImplementation *self);

/**
 * mcp_implementation_new_from_json:
 * @node: a #JsonNode
 * @error: (nullable): return location for a #GError
 *
 * Creates implementation info from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpImplementation, or %NULL
 */
McpImplementation *mcp_implementation_new_from_json (JsonNode  *node,
                                                     GError   **error);

/* ========================================================================== */
/* McpServerCapabilities                                                      */
/* ========================================================================== */

#define MCP_TYPE_SERVER_CAPABILITIES (mcp_server_capabilities_get_type ())

G_DECLARE_FINAL_TYPE (McpServerCapabilities, mcp_server_capabilities, MCP, SERVER_CAPABILITIES, GObject)

/**
 * mcp_server_capabilities_new:
 *
 * Creates a new server capabilities object with all capabilities disabled.
 *
 * Returns: (transfer full): a new #McpServerCapabilities
 */
McpServerCapabilities *mcp_server_capabilities_new (void);

/* Logging capability */

/**
 * mcp_server_capabilities_get_logging:
 * @self: an #McpServerCapabilities
 *
 * Gets whether logging is supported.
 *
 * Returns: %TRUE if logging is supported
 */
gboolean mcp_server_capabilities_get_logging (McpServerCapabilities *self);

/**
 * mcp_server_capabilities_set_logging:
 * @self: an #McpServerCapabilities
 * @enabled: whether logging is supported
 *
 * Sets whether logging is supported.
 */
void mcp_server_capabilities_set_logging (McpServerCapabilities *self,
                                          gboolean               enabled);

/* Prompts capability */

/**
 * mcp_server_capabilities_get_prompts:
 * @self: an #McpServerCapabilities
 *
 * Gets whether prompts are supported.
 *
 * Returns: %TRUE if prompts are supported
 */
gboolean mcp_server_capabilities_get_prompts (McpServerCapabilities *self);

/**
 * mcp_server_capabilities_set_prompts:
 * @self: an #McpServerCapabilities
 * @enabled: whether prompts are supported
 * @list_changed: whether list change notifications are supported
 *
 * Sets prompts capability.
 */
void mcp_server_capabilities_set_prompts (McpServerCapabilities *self,
                                          gboolean               enabled,
                                          gboolean               list_changed);

/**
 * mcp_server_capabilities_get_prompts_list_changed:
 * @self: an #McpServerCapabilities
 *
 * Gets whether prompt list change notifications are supported.
 *
 * Returns: %TRUE if supported
 */
gboolean mcp_server_capabilities_get_prompts_list_changed (McpServerCapabilities *self);

/* Resources capability */

/**
 * mcp_server_capabilities_get_resources:
 * @self: an #McpServerCapabilities
 *
 * Gets whether resources are supported.
 *
 * Returns: %TRUE if resources are supported
 */
gboolean mcp_server_capabilities_get_resources (McpServerCapabilities *self);

/**
 * mcp_server_capabilities_set_resources:
 * @self: an #McpServerCapabilities
 * @enabled: whether resources are supported
 * @subscribe: whether subscription is supported
 * @list_changed: whether list change notifications are supported
 *
 * Sets resources capability.
 */
void mcp_server_capabilities_set_resources (McpServerCapabilities *self,
                                            gboolean               enabled,
                                            gboolean               subscribe,
                                            gboolean               list_changed);

/**
 * mcp_server_capabilities_get_resources_subscribe:
 * @self: an #McpServerCapabilities
 *
 * Gets whether resource subscription is supported.
 *
 * Returns: %TRUE if supported
 */
gboolean mcp_server_capabilities_get_resources_subscribe (McpServerCapabilities *self);

/**
 * mcp_server_capabilities_get_resources_list_changed:
 * @self: an #McpServerCapabilities
 *
 * Gets whether resource list change notifications are supported.
 *
 * Returns: %TRUE if supported
 */
gboolean mcp_server_capabilities_get_resources_list_changed (McpServerCapabilities *self);

/* Tools capability */

/**
 * mcp_server_capabilities_get_tools:
 * @self: an #McpServerCapabilities
 *
 * Gets whether tools are supported.
 *
 * Returns: %TRUE if tools are supported
 */
gboolean mcp_server_capabilities_get_tools (McpServerCapabilities *self);

/**
 * mcp_server_capabilities_set_tools:
 * @self: an #McpServerCapabilities
 * @enabled: whether tools are supported
 * @list_changed: whether list change notifications are supported
 *
 * Sets tools capability.
 */
void mcp_server_capabilities_set_tools (McpServerCapabilities *self,
                                        gboolean               enabled,
                                        gboolean               list_changed);

/**
 * mcp_server_capabilities_get_tools_list_changed:
 * @self: an #McpServerCapabilities
 *
 * Gets whether tool list change notifications are supported.
 *
 * Returns: %TRUE if supported
 */
gboolean mcp_server_capabilities_get_tools_list_changed (McpServerCapabilities *self);

/* Completions capability */

/**
 * mcp_server_capabilities_get_completions:
 * @self: an #McpServerCapabilities
 *
 * Gets whether completions are supported.
 *
 * Returns: %TRUE if completions are supported
 */
gboolean mcp_server_capabilities_get_completions (McpServerCapabilities *self);

/**
 * mcp_server_capabilities_set_completions:
 * @self: an #McpServerCapabilities
 * @enabled: whether completions are supported
 *
 * Sets whether completions are supported.
 */
void mcp_server_capabilities_set_completions (McpServerCapabilities *self,
                                              gboolean               enabled);

/* Tasks capability (experimental) */

/**
 * mcp_server_capabilities_get_tasks:
 * @self: an #McpServerCapabilities
 *
 * Gets whether tasks are supported.
 *
 * Returns: %TRUE if tasks are supported
 */
gboolean mcp_server_capabilities_get_tasks (McpServerCapabilities *self);

/**
 * mcp_server_capabilities_set_tasks:
 * @self: an #McpServerCapabilities
 * @enabled: whether tasks are supported
 *
 * Sets whether tasks are supported.
 */
void mcp_server_capabilities_set_tasks (McpServerCapabilities *self,
                                        gboolean               enabled);

/* Experimental capabilities */

/**
 * mcp_server_capabilities_set_experimental:
 * @self: an #McpServerCapabilities
 * @name: the experimental feature name
 * @config: (nullable): configuration for the feature
 *
 * Sets an experimental capability.
 */
void mcp_server_capabilities_set_experimental (McpServerCapabilities *self,
                                               const gchar           *name,
                                               JsonNode              *config);

/**
 * mcp_server_capabilities_get_experimental:
 * @self: an #McpServerCapabilities
 * @name: the experimental feature name
 *
 * Gets an experimental capability configuration.
 *
 * Returns: (transfer none) (nullable): the configuration, or %NULL
 */
JsonNode *mcp_server_capabilities_get_experimental (McpServerCapabilities *self,
                                                    const gchar           *name);

/* JSON serialization */

/**
 * mcp_server_capabilities_to_json:
 * @self: an #McpServerCapabilities
 *
 * Serializes the capabilities to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_server_capabilities_to_json (McpServerCapabilities *self);

/**
 * mcp_server_capabilities_new_from_json:
 * @node: a #JsonNode
 * @error: (nullable): return location for a #GError
 *
 * Creates capabilities from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpServerCapabilities, or %NULL
 */
McpServerCapabilities *mcp_server_capabilities_new_from_json (JsonNode  *node,
                                                               GError   **error);

/* ========================================================================== */
/* McpClientCapabilities                                                      */
/* ========================================================================== */

#define MCP_TYPE_CLIENT_CAPABILITIES (mcp_client_capabilities_get_type ())

G_DECLARE_FINAL_TYPE (McpClientCapabilities, mcp_client_capabilities, MCP, CLIENT_CAPABILITIES, GObject)

/**
 * mcp_client_capabilities_new:
 *
 * Creates a new client capabilities object with all capabilities disabled.
 *
 * Returns: (transfer full): a new #McpClientCapabilities
 */
McpClientCapabilities *mcp_client_capabilities_new (void);

/* Sampling capability */

/**
 * mcp_client_capabilities_get_sampling:
 * @self: an #McpClientCapabilities
 *
 * Gets whether sampling is supported.
 *
 * Returns: %TRUE if sampling is supported
 */
gboolean mcp_client_capabilities_get_sampling (McpClientCapabilities *self);

/**
 * mcp_client_capabilities_set_sampling:
 * @self: an #McpClientCapabilities
 * @enabled: whether sampling is supported
 *
 * Sets whether sampling is supported.
 */
void mcp_client_capabilities_set_sampling (McpClientCapabilities *self,
                                           gboolean               enabled);

/* Roots capability */

/**
 * mcp_client_capabilities_get_roots:
 * @self: an #McpClientCapabilities
 *
 * Gets whether roots are supported.
 *
 * Returns: %TRUE if roots are supported
 */
gboolean mcp_client_capabilities_get_roots (McpClientCapabilities *self);

/**
 * mcp_client_capabilities_set_roots:
 * @self: an #McpClientCapabilities
 * @enabled: whether roots are supported
 * @list_changed: whether list change notifications are supported
 *
 * Sets roots capability.
 */
void mcp_client_capabilities_set_roots (McpClientCapabilities *self,
                                        gboolean               enabled,
                                        gboolean               list_changed);

/**
 * mcp_client_capabilities_get_roots_list_changed:
 * @self: an #McpClientCapabilities
 *
 * Gets whether roots list change notifications are supported.
 *
 * Returns: %TRUE if supported
 */
gboolean mcp_client_capabilities_get_roots_list_changed (McpClientCapabilities *self);

/* Elicitation capability */

/**
 * mcp_client_capabilities_get_elicitation:
 * @self: an #McpClientCapabilities
 *
 * Gets whether elicitation is supported.
 *
 * Returns: %TRUE if elicitation is supported
 */
gboolean mcp_client_capabilities_get_elicitation (McpClientCapabilities *self);

/**
 * mcp_client_capabilities_set_elicitation:
 * @self: an #McpClientCapabilities
 * @enabled: whether elicitation is supported
 *
 * Sets whether elicitation is supported.
 */
void mcp_client_capabilities_set_elicitation (McpClientCapabilities *self,
                                              gboolean               enabled);

/* Tasks capability (experimental) */

/**
 * mcp_client_capabilities_get_tasks:
 * @self: an #McpClientCapabilities
 *
 * Gets whether tasks are supported.
 *
 * Returns: %TRUE if tasks are supported
 */
gboolean mcp_client_capabilities_get_tasks (McpClientCapabilities *self);

/**
 * mcp_client_capabilities_set_tasks:
 * @self: an #McpClientCapabilities
 * @enabled: whether tasks are supported
 *
 * Sets whether tasks are supported.
 */
void mcp_client_capabilities_set_tasks (McpClientCapabilities *self,
                                        gboolean               enabled);

/* Experimental capabilities */

/**
 * mcp_client_capabilities_set_experimental:
 * @self: an #McpClientCapabilities
 * @name: the experimental feature name
 * @config: (nullable): configuration for the feature
 *
 * Sets an experimental capability.
 */
void mcp_client_capabilities_set_experimental (McpClientCapabilities *self,
                                               const gchar           *name,
                                               JsonNode              *config);

/**
 * mcp_client_capabilities_get_experimental:
 * @self: an #McpClientCapabilities
 * @name: the experimental feature name
 *
 * Gets an experimental capability configuration.
 *
 * Returns: (transfer none) (nullable): the configuration, or %NULL
 */
JsonNode *mcp_client_capabilities_get_experimental (McpClientCapabilities *self,
                                                    const gchar           *name);

/* JSON serialization */

/**
 * mcp_client_capabilities_to_json:
 * @self: an #McpClientCapabilities
 *
 * Serializes the capabilities to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_client_capabilities_to_json (McpClientCapabilities *self);

/**
 * mcp_client_capabilities_new_from_json:
 * @node: a #JsonNode
 * @error: (nullable): return location for a #GError
 *
 * Creates capabilities from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpClientCapabilities, or %NULL
 */
McpClientCapabilities *mcp_client_capabilities_new_from_json (JsonNode  *node,
                                                               GError   **error);

G_END_DECLS

#endif /* MCP_CAPABILITIES_H */
