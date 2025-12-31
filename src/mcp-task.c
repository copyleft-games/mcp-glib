/*
 * mcp-task.c - Task implementation for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This implements the experimental Tasks API from MCP specification.
 */
#include "mcp.h"

typedef struct
{
    gchar         *task_id;
    McpTaskStatus  status;
    gchar         *status_message;
    GDateTime     *created_at;
    GDateTime     *last_updated_at;
    gint64         ttl;
    gint64         poll_interval;
} McpTaskPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (McpTask, mcp_task, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_TASK_ID,
    PROP_STATUS,
    PROP_STATUS_MESSAGE,
    PROP_CREATED_AT,
    PROP_LAST_UPDATED_AT,
    PROP_TTL,
    PROP_POLL_INTERVAL,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
mcp_task_finalize (GObject *object)
{
    McpTask *self = MCP_TASK (object);
    McpTaskPrivate *priv = mcp_task_get_instance_private (self);

    g_clear_pointer (&priv->task_id, g_free);
    g_clear_pointer (&priv->status_message, g_free);
    g_clear_pointer (&priv->created_at, g_date_time_unref);
    g_clear_pointer (&priv->last_updated_at, g_date_time_unref);

    G_OBJECT_CLASS (mcp_task_parent_class)->finalize (object);
}

static void
mcp_task_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
    McpTask *self = MCP_TASK (object);
    McpTaskPrivate *priv = mcp_task_get_instance_private (self);

    switch (prop_id)
    {
    case PROP_TASK_ID:
        g_value_set_string (value, priv->task_id);
        break;
    case PROP_STATUS:
        g_value_set_enum (value, priv->status);
        break;
    case PROP_STATUS_MESSAGE:
        g_value_set_string (value, priv->status_message);
        break;
    case PROP_CREATED_AT:
        g_value_set_boxed (value, priv->created_at);
        break;
    case PROP_LAST_UPDATED_AT:
        g_value_set_boxed (value, priv->last_updated_at);
        break;
    case PROP_TTL:
        g_value_set_int64 (value, priv->ttl);
        break;
    case PROP_POLL_INTERVAL:
        g_value_set_int64 (value, priv->poll_interval);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mcp_task_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    McpTask *self = MCP_TASK (object);
    McpTaskPrivate *priv = mcp_task_get_instance_private (self);

    switch (prop_id)
    {
    case PROP_TASK_ID:
        g_free (priv->task_id);
        priv->task_id = g_value_dup_string (value);
        break;
    case PROP_STATUS:
        priv->status = g_value_get_enum (value);
        break;
    case PROP_STATUS_MESSAGE:
        g_free (priv->status_message);
        priv->status_message = g_value_dup_string (value);
        break;
    case PROP_CREATED_AT:
        g_clear_pointer (&priv->created_at, g_date_time_unref);
        priv->created_at = g_value_dup_boxed (value);
        break;
    case PROP_LAST_UPDATED_AT:
        g_clear_pointer (&priv->last_updated_at, g_date_time_unref);
        priv->last_updated_at = g_value_dup_boxed (value);
        break;
    case PROP_TTL:
        priv->ttl = g_value_get_int64 (value);
        break;
    case PROP_POLL_INTERVAL:
        priv->poll_interval = g_value_get_int64 (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mcp_task_class_init (McpTaskClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = mcp_task_finalize;
    object_class->get_property = mcp_task_get_property;
    object_class->set_property = mcp_task_set_property;

    /**
     * McpTask:task-id:
     *
     * The unique identifier for the task.
     */
    properties[PROP_TASK_ID] =
        g_param_spec_string ("task-id",
                             "Task ID",
                             "The unique identifier for the task",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    /**
     * McpTask:status:
     *
     * The current status of the task.
     */
    properties[PROP_STATUS] =
        g_param_spec_enum ("status",
                           "Status",
                           "The current status of the task",
                           MCP_TYPE_TASK_STATUS,
                           MCP_TASK_STATUS_WORKING,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    /**
     * McpTask:status-message:
     *
     * Human-readable message describing the current task state.
     */
    properties[PROP_STATUS_MESSAGE] =
        g_param_spec_string ("status-message",
                             "Status Message",
                             "Human-readable message describing the task state",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpTask:created-at:
     *
     * Timestamp when the task was created.
     */
    properties[PROP_CREATED_AT] =
        g_param_spec_boxed ("created-at",
                            "Created At",
                            "Timestamp when the task was created",
                            G_TYPE_DATE_TIME,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpTask:last-updated-at:
     *
     * Timestamp when the task was last updated.
     */
    properties[PROP_LAST_UPDATED_AT] =
        g_param_spec_boxed ("last-updated-at",
                            "Last Updated At",
                            "Timestamp when the task was last updated",
                            G_TYPE_DATE_TIME,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpTask:ttl:
     *
     * Task time-to-live in milliseconds. -1 for unlimited.
     */
    properties[PROP_TTL] =
        g_param_spec_int64 ("ttl",
                            "TTL",
                            "Task time-to-live in milliseconds",
                            -1, G_MAXINT64, -1,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpTask:poll-interval:
     *
     * Suggested polling interval in milliseconds. -1 if not set.
     */
    properties[PROP_POLL_INTERVAL] =
        g_param_spec_int64 ("poll-interval",
                            "Poll Interval",
                            "Suggested polling interval in milliseconds",
                            -1, G_MAXINT64, -1,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mcp_task_init (McpTask *self)
{
    McpTaskPrivate *priv = mcp_task_get_instance_private (self);

    priv->status = MCP_TASK_STATUS_WORKING;
    priv->ttl = -1;
    priv->poll_interval = -1;

    /* Set default timestamps to now */
    priv->created_at = g_date_time_new_now_utc ();
    priv->last_updated_at = g_date_time_new_now_utc ();
}

/**
 * mcp_task_new:
 * @task_id: the task identifier
 * @status: the initial task status
 *
 * Creates a new task.
 *
 * Returns: (transfer full): a new #McpTask
 */
McpTask *
mcp_task_new (const gchar   *task_id,
              McpTaskStatus  status)
{
    return g_object_new (MCP_TYPE_TASK,
                         "task-id", task_id,
                         "status", status,
                         NULL);
}

/**
 * mcp_task_get_task_id:
 * @self: an #McpTask
 *
 * Gets the task identifier.
 *
 * Returns: (transfer none): the task ID
 */
const gchar *
mcp_task_get_task_id (McpTask *self)
{
    McpTaskPrivate *priv;

    g_return_val_if_fail (MCP_IS_TASK (self), NULL);

    priv = mcp_task_get_instance_private (self);
    return priv->task_id;
}

/**
 * mcp_task_set_task_id:
 * @self: an #McpTask
 * @task_id: the task ID
 *
 * Sets the task identifier.
 */
void
mcp_task_set_task_id (McpTask     *self,
                      const gchar *task_id)
{
    g_return_if_fail (MCP_IS_TASK (self));
    g_object_set (self, "task-id", task_id, NULL);
}

/**
 * mcp_task_get_status:
 * @self: an #McpTask
 *
 * Gets the current task status.
 *
 * Returns: the task status
 */
McpTaskStatus
mcp_task_get_status (McpTask *self)
{
    McpTaskPrivate *priv;

    g_return_val_if_fail (MCP_IS_TASK (self), MCP_TASK_STATUS_WORKING);

    priv = mcp_task_get_instance_private (self);
    return priv->status;
}

/**
 * mcp_task_set_status:
 * @self: an #McpTask
 * @status: the task status
 *
 * Sets the task status.
 */
void
mcp_task_set_status (McpTask       *self,
                     McpTaskStatus  status)
{
    g_return_if_fail (MCP_IS_TASK (self));
    g_object_set (self, "status", status, NULL);
}

/**
 * mcp_task_get_status_message:
 * @self: an #McpTask
 *
 * Gets the human-readable status message.
 *
 * Returns: (transfer none) (nullable): the status message
 */
const gchar *
mcp_task_get_status_message (McpTask *self)
{
    McpTaskPrivate *priv;

    g_return_val_if_fail (MCP_IS_TASK (self), NULL);

    priv = mcp_task_get_instance_private (self);
    return priv->status_message;
}

/**
 * mcp_task_set_status_message:
 * @self: an #McpTask
 * @message: (nullable): the status message
 *
 * Sets the human-readable status message.
 */
void
mcp_task_set_status_message (McpTask     *self,
                             const gchar *message)
{
    g_return_if_fail (MCP_IS_TASK (self));
    g_object_set (self, "status-message", message, NULL);
}

/**
 * mcp_task_get_created_at:
 * @self: an #McpTask
 *
 * Gets the creation timestamp.
 *
 * Returns: (transfer none): the creation timestamp
 */
GDateTime *
mcp_task_get_created_at (McpTask *self)
{
    McpTaskPrivate *priv;

    g_return_val_if_fail (MCP_IS_TASK (self), NULL);

    priv = mcp_task_get_instance_private (self);
    return priv->created_at;
}

/**
 * mcp_task_set_created_at:
 * @self: an #McpTask
 * @created_at: the creation timestamp
 *
 * Sets the creation timestamp.
 */
void
mcp_task_set_created_at (McpTask   *self,
                         GDateTime *created_at)
{
    g_return_if_fail (MCP_IS_TASK (self));
    g_object_set (self, "created-at", created_at, NULL);
}

/**
 * mcp_task_get_last_updated_at:
 * @self: an #McpTask
 *
 * Gets the last updated timestamp.
 *
 * Returns: (transfer none): the last updated timestamp
 */
GDateTime *
mcp_task_get_last_updated_at (McpTask *self)
{
    McpTaskPrivate *priv;

    g_return_val_if_fail (MCP_IS_TASK (self), NULL);

    priv = mcp_task_get_instance_private (self);
    return priv->last_updated_at;
}

/**
 * mcp_task_set_last_updated_at:
 * @self: an #McpTask
 * @last_updated_at: the last updated timestamp
 *
 * Sets the last updated timestamp.
 */
void
mcp_task_set_last_updated_at (McpTask   *self,
                              GDateTime *last_updated_at)
{
    g_return_if_fail (MCP_IS_TASK (self));
    g_object_set (self, "last-updated-at", last_updated_at, NULL);
}

/**
 * mcp_task_get_ttl:
 * @self: an #McpTask
 *
 * Gets the task time-to-live in milliseconds.
 *
 * Returns: the TTL in milliseconds, or -1 for unlimited
 */
gint64
mcp_task_get_ttl (McpTask *self)
{
    McpTaskPrivate *priv;

    g_return_val_if_fail (MCP_IS_TASK (self), -1);

    priv = mcp_task_get_instance_private (self);
    return priv->ttl;
}

/**
 * mcp_task_set_ttl:
 * @self: an #McpTask
 * @ttl: the TTL in milliseconds, or -1 for unlimited
 *
 * Sets the task time-to-live.
 */
void
mcp_task_set_ttl (McpTask *self,
                  gint64   ttl)
{
    g_return_if_fail (MCP_IS_TASK (self));
    g_object_set (self, "ttl", ttl, NULL);
}

/**
 * mcp_task_get_poll_interval:
 * @self: an #McpTask
 *
 * Gets the suggested polling interval in milliseconds.
 *
 * Returns: the poll interval in milliseconds, or -1 if not set
 */
gint64
mcp_task_get_poll_interval (McpTask *self)
{
    McpTaskPrivate *priv;

    g_return_val_if_fail (MCP_IS_TASK (self), -1);

    priv = mcp_task_get_instance_private (self);
    return priv->poll_interval;
}

/**
 * mcp_task_set_poll_interval:
 * @self: an #McpTask
 * @poll_interval: the poll interval in milliseconds, or -1 if not set
 *
 * Sets the suggested polling interval.
 */
void
mcp_task_set_poll_interval (McpTask *self,
                            gint64   poll_interval)
{
    g_return_if_fail (MCP_IS_TASK (self));
    g_object_set (self, "poll-interval", poll_interval, NULL);
}

/**
 * mcp_task_update_timestamp:
 * @self: an #McpTask
 *
 * Updates the last_updated_at timestamp to the current time.
 */
void
mcp_task_update_timestamp (McpTask *self)
{
    McpTaskPrivate *priv;

    g_return_if_fail (MCP_IS_TASK (self));

    priv = mcp_task_get_instance_private (self);
    g_clear_pointer (&priv->last_updated_at, g_date_time_unref);
    priv->last_updated_at = g_date_time_new_now_utc ();
}

/**
 * mcp_task_to_json:
 * @self: an #McpTask
 *
 * Serializes the task to a JSON object for MCP protocol.
 *
 * Returns: (transfer full): a #JsonNode containing the task
 */
JsonNode *
mcp_task_to_json (McpTask *self)
{
    McpTaskPrivate *priv;
    JsonBuilder *builder;
    JsonNode *result;
    gchar *iso_str;

    g_return_val_if_fail (MCP_IS_TASK (self), NULL);

    priv = mcp_task_get_instance_private (self);

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    /* Required: taskId */
    json_builder_set_member_name (builder, "taskId");
    json_builder_add_string_value (builder, priv->task_id ? priv->task_id : "");

    /* Required: status */
    json_builder_set_member_name (builder, "status");
    json_builder_add_string_value (builder, mcp_task_status_to_string (priv->status));

    /* Optional: statusMessage */
    if (priv->status_message != NULL)
    {
        json_builder_set_member_name (builder, "statusMessage");
        json_builder_add_string_value (builder, priv->status_message);
    }

    /* Required: createdAt (ISO 8601) */
    json_builder_set_member_name (builder, "createdAt");
    iso_str = g_date_time_format_iso8601 (priv->created_at);
    json_builder_add_string_value (builder, iso_str);
    g_free (iso_str);

    /* Required: lastUpdatedAt (ISO 8601) */
    json_builder_set_member_name (builder, "lastUpdatedAt");
    iso_str = g_date_time_format_iso8601 (priv->last_updated_at);
    json_builder_add_string_value (builder, iso_str);
    g_free (iso_str);

    /* Optional: ttl (only if not -1) */
    if (priv->ttl >= 0)
    {
        json_builder_set_member_name (builder, "ttl");
        json_builder_add_int_value (builder, priv->ttl);
    }
    else
    {
        /* null for unlimited */
        json_builder_set_member_name (builder, "ttl");
        json_builder_add_null_value (builder);
    }

    /* Optional: pollInterval (only if set) */
    if (priv->poll_interval >= 0)
    {
        json_builder_set_member_name (builder, "pollInterval");
        json_builder_add_int_value (builder, priv->poll_interval);
    }

    json_builder_end_object (builder);

    result = json_builder_get_root (builder);
    g_object_unref (builder);

    return result;
}

/**
 * mcp_task_new_from_json:
 * @node: a #JsonNode containing a task definition
 * @error: (nullable): return location for a #GError
 *
 * Creates a new task from a JSON definition.
 *
 * Returns: (transfer full) (nullable): a new #McpTask, or %NULL on error
 */
McpTask *
mcp_task_new_from_json (JsonNode  *node,
                        GError   **error)
{
    JsonObject *obj;
    McpTask *task;
    const gchar *task_id;
    const gchar *status_str;
    McpTaskStatus status;
    GDateTime *dt;
    GTimeZone *utc;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Task definition must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    /* Required: taskId */
    if (!json_object_has_member (obj, "taskId"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Task definition missing required 'taskId' field");
        return NULL;
    }
    task_id = json_object_get_string_member (obj, "taskId");

    /* Required: status */
    if (!json_object_has_member (obj, "status"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Task definition missing required 'status' field");
        return NULL;
    }
    status_str = json_object_get_string_member (obj, "status");
    status = mcp_task_status_from_string (status_str);

    task = mcp_task_new (task_id, status);

    /* Optional: statusMessage */
    if (json_object_has_member (obj, "statusMessage"))
        mcp_task_set_status_message (task, json_object_get_string_member (obj, "statusMessage"));

    /* Required: createdAt */
    if (json_object_has_member (obj, "createdAt"))
    {
        const gchar *created_str = json_object_get_string_member (obj, "createdAt");
        utc = g_time_zone_new_utc ();
        dt = g_date_time_new_from_iso8601 (created_str, utc);
        g_time_zone_unref (utc);
        if (dt != NULL)
        {
            mcp_task_set_created_at (task, dt);
            g_date_time_unref (dt);
        }
    }

    /* Required: lastUpdatedAt */
    if (json_object_has_member (obj, "lastUpdatedAt"))
    {
        const gchar *updated_str = json_object_get_string_member (obj, "lastUpdatedAt");
        utc = g_time_zone_new_utc ();
        dt = g_date_time_new_from_iso8601 (updated_str, utc);
        g_time_zone_unref (utc);
        if (dt != NULL)
        {
            mcp_task_set_last_updated_at (task, dt);
            g_date_time_unref (dt);
        }
    }

    /* Optional: ttl (can be null) */
    if (json_object_has_member (obj, "ttl"))
    {
        JsonNode *ttl_node = json_object_get_member (obj, "ttl");
        if (json_node_is_null (ttl_node))
            mcp_task_set_ttl (task, -1);
        else
            mcp_task_set_ttl (task, json_node_get_int (ttl_node));
    }

    /* Optional: pollInterval */
    if (json_object_has_member (obj, "pollInterval"))
        mcp_task_set_poll_interval (task, json_object_get_int_member (obj, "pollInterval"));

    return task;
}
