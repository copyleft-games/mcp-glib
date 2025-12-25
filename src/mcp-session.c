/*
 * mcp-session.c - Session base class implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define MCP_COMPILATION
#include <mcp/mcp-session.h>
#include <mcp/mcp-error.h>
#undef MCP_COMPILATION

/*
 * Private data structure for McpSession.
 * This is stored separately and accessed via mcp_session_get_instance_private().
 */
typedef struct
{
    McpSessionState    state;
    gchar             *protocol_version;
    McpImplementation *local_impl;
    McpImplementation *remote_impl;

    /* Request ID counter for generating unique IDs */
    guint64 next_request_id;

    /* Pending requests: request_id -> GTask */
    GHashTable *pending_requests;
} McpSessionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (McpSession, mcp_session, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_STATE,
    PROP_PROTOCOL_VERSION,
    PROP_LOCAL_IMPLEMENTATION,
    PROP_REMOTE_IMPLEMENTATION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

enum
{
    SIGNAL_STATE_CHANGED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
mcp_session_finalize (GObject *object)
{
    McpSession *self = MCP_SESSION (object);
    McpSessionPrivate *priv = mcp_session_get_instance_private (self);

    g_clear_pointer (&priv->protocol_version, g_free);
    g_clear_object (&priv->local_impl);
    g_clear_object (&priv->remote_impl);
    g_clear_pointer (&priv->pending_requests, g_hash_table_unref);

    G_OBJECT_CLASS (mcp_session_parent_class)->finalize (object);
}

static void
mcp_session_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    McpSession *self = MCP_SESSION (object);
    McpSessionPrivate *priv = mcp_session_get_instance_private (self);

    switch (prop_id)
    {
        case PROP_STATE:
            g_value_set_int (value, priv->state);
            break;
        case PROP_PROTOCOL_VERSION:
            g_value_set_string (value, priv->protocol_version);
            break;
        case PROP_LOCAL_IMPLEMENTATION:
            g_value_set_object (value, priv->local_impl);
            break;
        case PROP_REMOTE_IMPLEMENTATION:
            g_value_set_object (value, priv->remote_impl);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_session_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    McpSession *self = MCP_SESSION (object);
    McpSessionPrivate *priv = mcp_session_get_instance_private (self);

    switch (prop_id)
    {
        case PROP_LOCAL_IMPLEMENTATION:
            g_clear_object (&priv->local_impl);
            priv->local_impl = g_value_dup_object (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_session_class_init (McpSessionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = mcp_session_finalize;
    object_class->get_property = mcp_session_get_property;
    object_class->set_property = mcp_session_set_property;

    /**
     * McpSession:state:
     *
     * The current session state.
     */
    properties[PROP_STATE] =
        g_param_spec_int ("state",
                          "State",
                          "The current session state",
                          MCP_SESSION_STATE_DISCONNECTED,
                          MCP_SESSION_STATE_ERROR,
                          MCP_SESSION_STATE_DISCONNECTED,
                          G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS);

    /**
     * McpSession:protocol-version:
     *
     * The negotiated protocol version.
     */
    properties[PROP_PROTOCOL_VERSION] =
        g_param_spec_string ("protocol-version",
                             "Protocol Version",
                             "The negotiated protocol version",
                             NULL,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    /**
     * McpSession:local-implementation:
     *
     * The local implementation info.
     */
    properties[PROP_LOCAL_IMPLEMENTATION] =
        g_param_spec_object ("local-implementation",
                             "Local Implementation",
                             "The local implementation info",
                             MCP_TYPE_IMPLEMENTATION,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    /**
     * McpSession:remote-implementation:
     *
     * The remote peer's implementation info.
     */
    properties[PROP_REMOTE_IMPLEMENTATION] =
        g_param_spec_object ("remote-implementation",
                             "Remote Implementation",
                             "The remote peer's implementation info",
                             MCP_TYPE_IMPLEMENTATION,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    /**
     * McpSession::state-changed:
     * @self: the #McpSession
     * @old_state: the previous state
     * @new_state: the new state
     *
     * Emitted when the session state changes.
     */
    signals[SIGNAL_STATE_CHANGED] =
        g_signal_new ("state-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE,
                      2,
                      G_TYPE_INT,
                      G_TYPE_INT);
}

static void
mcp_session_init (McpSession *self)
{
    McpSessionPrivate *priv = mcp_session_get_instance_private (self);

    priv->state = MCP_SESSION_STATE_DISCONNECTED;
    priv->next_request_id = 1;
    priv->pending_requests = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     g_free, g_object_unref);
}

/**
 * mcp_session_get_state:
 * @self: an #McpSession
 *
 * Gets the current session state.
 *
 * Returns: the #McpSessionState
 */
McpSessionState
mcp_session_get_state (McpSession *self)
{
    McpSessionPrivate *priv;

    g_return_val_if_fail (MCP_IS_SESSION (self), MCP_SESSION_STATE_DISCONNECTED);

    priv = mcp_session_get_instance_private (self);
    return priv->state;
}

/*
 * Internal function to set state and emit signal.
 */
void
mcp_session_set_state (McpSession      *self,
                       McpSessionState  new_state)
{
    McpSessionPrivate *priv;
    McpSessionState old_state;

    g_return_if_fail (MCP_IS_SESSION (self));

    priv = mcp_session_get_instance_private (self);
    old_state = priv->state;

    if (old_state != new_state)
    {
        priv->state = new_state;
        g_signal_emit (self, signals[SIGNAL_STATE_CHANGED], 0, old_state, new_state);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
    }
}

/**
 * mcp_session_get_protocol_version:
 * @self: an #McpSession
 *
 * Gets the negotiated protocol version.
 *
 * Returns: (transfer none) (nullable): the protocol version, or %NULL
 */
const gchar *
mcp_session_get_protocol_version (McpSession *self)
{
    McpSessionPrivate *priv;

    g_return_val_if_fail (MCP_IS_SESSION (self), NULL);

    priv = mcp_session_get_instance_private (self);
    return priv->protocol_version;
}

/*
 * Internal function to set protocol version.
 */
void
mcp_session_set_protocol_version (McpSession  *self,
                                  const gchar *version)
{
    McpSessionPrivate *priv;

    g_return_if_fail (MCP_IS_SESSION (self));

    priv = mcp_session_get_instance_private (self);
    g_free (priv->protocol_version);
    priv->protocol_version = g_strdup (version);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROTOCOL_VERSION]);
}

/**
 * mcp_session_get_local_implementation:
 * @self: an #McpSession
 *
 * Gets the local implementation info.
 *
 * Returns: (transfer none) (nullable): the local #McpImplementation
 */
McpImplementation *
mcp_session_get_local_implementation (McpSession *self)
{
    McpSessionPrivate *priv;

    g_return_val_if_fail (MCP_IS_SESSION (self), NULL);

    priv = mcp_session_get_instance_private (self);
    return priv->local_impl;
}

/**
 * mcp_session_set_local_implementation:
 * @self: an #McpSession
 * @impl: (transfer none): the implementation info
 *
 * Sets the local implementation info.
 */
void
mcp_session_set_local_implementation (McpSession        *self,
                                      McpImplementation *impl)
{
    McpSessionPrivate *priv;

    g_return_if_fail (MCP_IS_SESSION (self));
    g_return_if_fail (MCP_IS_IMPLEMENTATION (impl));

    priv = mcp_session_get_instance_private (self);
    g_clear_object (&priv->local_impl);
    priv->local_impl = g_object_ref (impl);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOCAL_IMPLEMENTATION]);
}

/**
 * mcp_session_get_remote_implementation:
 * @self: an #McpSession
 *
 * Gets the remote peer's implementation info.
 *
 * Returns: (transfer none) (nullable): the remote #McpImplementation
 */
McpImplementation *
mcp_session_get_remote_implementation (McpSession *self)
{
    McpSessionPrivate *priv;

    g_return_val_if_fail (MCP_IS_SESSION (self), NULL);

    priv = mcp_session_get_instance_private (self);
    return priv->remote_impl;
}

/*
 * Internal function to set remote implementation.
 */
void
mcp_session_set_remote_implementation (McpSession        *self,
                                       McpImplementation *impl)
{
    McpSessionPrivate *priv;

    g_return_if_fail (MCP_IS_SESSION (self));

    priv = mcp_session_get_instance_private (self);
    g_clear_object (&priv->remote_impl);
    if (impl != NULL)
    {
        priv->remote_impl = g_object_ref (impl);
    }
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REMOTE_IMPLEMENTATION]);
}

/**
 * mcp_session_generate_request_id:
 * @self: an #McpSession
 *
 * Generates a unique request ID for this session.
 *
 * Returns: (transfer full): a new request ID string
 */
gchar *
mcp_session_generate_request_id (McpSession *self)
{
    McpSessionPrivate *priv;
    guint64 id;

    g_return_val_if_fail (MCP_IS_SESSION (self), NULL);

    priv = mcp_session_get_instance_private (self);
    id = priv->next_request_id++;

    return g_strdup_printf ("%" G_GUINT64_FORMAT, id);
}

/**
 * mcp_session_has_pending_request:
 * @self: an #McpSession
 * @request_id: the request ID to check
 *
 * Checks if there is a pending request with the given ID.
 *
 * Returns: %TRUE if a request with this ID is pending
 */
gboolean
mcp_session_has_pending_request (McpSession  *self,
                                 const gchar *request_id)
{
    McpSessionPrivate *priv;

    g_return_val_if_fail (MCP_IS_SESSION (self), FALSE);
    g_return_val_if_fail (request_id != NULL, FALSE);

    priv = mcp_session_get_instance_private (self);
    return g_hash_table_contains (priv->pending_requests, request_id);
}

/**
 * mcp_session_get_pending_request_count:
 * @self: an #McpSession
 *
 * Gets the number of pending requests.
 *
 * Returns: the number of pending requests
 */
guint
mcp_session_get_pending_request_count (McpSession *self)
{
    McpSessionPrivate *priv;

    g_return_val_if_fail (MCP_IS_SESSION (self), 0);

    priv = mcp_session_get_instance_private (self);
    return g_hash_table_size (priv->pending_requests);
}

/*
 * Internal function to add a pending request.
 */
void
mcp_session_add_pending_request (McpSession  *self,
                                 const gchar *request_id,
                                 GTask       *task)
{
    McpSessionPrivate *priv;

    g_return_if_fail (MCP_IS_SESSION (self));
    g_return_if_fail (request_id != NULL);
    g_return_if_fail (G_IS_TASK (task));

    priv = mcp_session_get_instance_private (self);
    g_hash_table_insert (priv->pending_requests,
                         g_strdup (request_id),
                         g_object_ref (task));
}

/*
 * Internal function to complete a pending request.
 * Returns the task if found, or NULL.
 */
GTask *
mcp_session_take_pending_request (McpSession  *self,
                                  const gchar *request_id)
{
    McpSessionPrivate *priv;
    GTask *task;

    g_return_val_if_fail (MCP_IS_SESSION (self), NULL);
    g_return_val_if_fail (request_id != NULL, NULL);

    priv = mcp_session_get_instance_private (self);

    task = g_hash_table_lookup (priv->pending_requests, request_id);
    if (task != NULL)
    {
        g_object_ref (task);
        g_hash_table_remove (priv->pending_requests, request_id);
    }

    return task;
}

/*
 * Internal function to cancel all pending requests.
 */
void
mcp_session_cancel_all_pending_requests (McpSession *self,
                                         GError     *error)
{
    McpSessionPrivate *priv;
    GHashTableIter iter;
    gpointer key, value;
    g_autoptr(GError) local_error = NULL;

    g_return_if_fail (MCP_IS_SESSION (self));

    priv = mcp_session_get_instance_private (self);

    if (error == NULL)
    {
        local_error = g_error_new (MCP_ERROR,
                                   MCP_ERROR_CONNECTION_CLOSED,
                                   "Session closed");
        error = local_error;
    }

    g_hash_table_iter_init (&iter, priv->pending_requests);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        GTask *task = G_TASK (value);
        g_task_return_error (task, g_error_copy (error));
    }

    g_hash_table_remove_all (priv->pending_requests);
}
