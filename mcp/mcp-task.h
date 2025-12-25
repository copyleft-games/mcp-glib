/*
 * mcp-task.h - Task definition for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This implements the experimental Tasks API from MCP specification.
 */

#ifndef MCP_TASK_H
#define MCP_TASK_H

#if !defined(MCP_INSIDE) && !defined(MCP_COMPILATION)
#error "Only <mcp/mcp.h> can be included directly."
#endif

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define MCP_TYPE_TASK (mcp_task_get_type ())

G_DECLARE_DERIVABLE_TYPE (McpTask, mcp_task, MCP, TASK, GObject)

/**
 * McpTaskClass:
 * @parent_class: the parent class
 *
 * The class structure for #McpTask.
 */
struct _McpTaskClass
{
    GObjectClass parent_class;

    /*< private >*/
    gpointer padding[8];
};

/**
 * mcp_task_new:
 * @task_id: the task identifier
 * @status: the initial task status
 *
 * Creates a new task.
 *
 * Returns: (transfer full): a new #McpTask
 */
McpTask *mcp_task_new (const gchar   *task_id,
                       McpTaskStatus  status);

/**
 * mcp_task_get_task_id:
 * @self: an #McpTask
 *
 * Gets the task identifier.
 *
 * Returns: (transfer none): the task ID
 */
const gchar *mcp_task_get_task_id (McpTask *self);

/**
 * mcp_task_set_task_id:
 * @self: an #McpTask
 * @task_id: the task ID
 *
 * Sets the task identifier.
 */
void mcp_task_set_task_id (McpTask     *self,
                           const gchar *task_id);

/**
 * mcp_task_get_status:
 * @self: an #McpTask
 *
 * Gets the current task status.
 *
 * Returns: the task status
 */
McpTaskStatus mcp_task_get_status (McpTask *self);

/**
 * mcp_task_set_status:
 * @self: an #McpTask
 * @status: the task status
 *
 * Sets the task status.
 */
void mcp_task_set_status (McpTask       *self,
                          McpTaskStatus  status);

/**
 * mcp_task_get_status_message:
 * @self: an #McpTask
 *
 * Gets the human-readable status message.
 *
 * Returns: (transfer none) (nullable): the status message
 */
const gchar *mcp_task_get_status_message (McpTask *self);

/**
 * mcp_task_set_status_message:
 * @self: an #McpTask
 * @message: (nullable): the status message
 *
 * Sets the human-readable status message.
 */
void mcp_task_set_status_message (McpTask     *self,
                                  const gchar *message);

/**
 * mcp_task_get_created_at:
 * @self: an #McpTask
 *
 * Gets the creation timestamp.
 *
 * Returns: (transfer none): the creation timestamp
 */
GDateTime *mcp_task_get_created_at (McpTask *self);

/**
 * mcp_task_set_created_at:
 * @self: an #McpTask
 * @created_at: the creation timestamp
 *
 * Sets the creation timestamp.
 */
void mcp_task_set_created_at (McpTask   *self,
                              GDateTime *created_at);

/**
 * mcp_task_get_last_updated_at:
 * @self: an #McpTask
 *
 * Gets the last updated timestamp.
 *
 * Returns: (transfer none): the last updated timestamp
 */
GDateTime *mcp_task_get_last_updated_at (McpTask *self);

/**
 * mcp_task_set_last_updated_at:
 * @self: an #McpTask
 * @last_updated_at: the last updated timestamp
 *
 * Sets the last updated timestamp.
 */
void mcp_task_set_last_updated_at (McpTask   *self,
                                   GDateTime *last_updated_at);

/**
 * mcp_task_get_ttl:
 * @self: an #McpTask
 *
 * Gets the task time-to-live in milliseconds.
 * Returns -1 if unlimited.
 *
 * Returns: the TTL in milliseconds, or -1 for unlimited
 */
gint64 mcp_task_get_ttl (McpTask *self);

/**
 * mcp_task_set_ttl:
 * @self: an #McpTask
 * @ttl: the TTL in milliseconds, or -1 for unlimited
 *
 * Sets the task time-to-live.
 */
void mcp_task_set_ttl (McpTask *self,
                       gint64   ttl);

/**
 * mcp_task_get_poll_interval:
 * @self: an #McpTask
 *
 * Gets the suggested polling interval in milliseconds.
 * Returns -1 if not set.
 *
 * Returns: the poll interval in milliseconds, or -1 if not set
 */
gint64 mcp_task_get_poll_interval (McpTask *self);

/**
 * mcp_task_set_poll_interval:
 * @self: an #McpTask
 * @poll_interval: the poll interval in milliseconds, or -1 if not set
 *
 * Sets the suggested polling interval.
 */
void mcp_task_set_poll_interval (McpTask *self,
                                 gint64   poll_interval);

/**
 * mcp_task_update_timestamp:
 * @self: an #McpTask
 *
 * Updates the last_updated_at timestamp to the current time.
 */
void mcp_task_update_timestamp (McpTask *self);

/**
 * mcp_task_to_json:
 * @self: an #McpTask
 *
 * Serializes the task to a JSON object for MCP protocol.
 *
 * Returns: (transfer full): a #JsonNode containing the task
 */
JsonNode *mcp_task_to_json (McpTask *self);

/**
 * mcp_task_new_from_json:
 * @node: a #JsonNode containing a task definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new task from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpTask, or %NULL on error
 */
McpTask *mcp_task_new_from_json (JsonNode  *node,
                                 GError   **error);

G_END_DECLS

#endif /* MCP_TASK_H */
