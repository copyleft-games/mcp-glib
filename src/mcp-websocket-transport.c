/*
 * mcp-websocket-transport.c - WebSocket transport implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp-websocket-transport.h"
#include "mcp-error.h"
#undef MCP_COMPILATION

#include <string.h>

/**
 * SECTION:mcp-websocket-transport
 * @title: McpWebSocketTransport
 * @short_description: WebSocket transport implementation
 *
 * #McpWebSocketTransport provides a WebSocket-based transport for MCP
 * communication. It provides full bidirectional communication using
 * WebSocket text frames for JSON-RPC messages.
 */

struct _McpWebSocketTransport
{
    GObject parent_instance;

    /* Configuration */
    gchar  *uri;
    gchar  *auth_token;
    gchar **protocols;
    gchar  *origin;
    gboolean reconnect_enabled;
    guint    reconnect_delay_ms;
    guint    max_reconnect_attempts;
    guint    keepalive_interval;

    /* Soup session */
    SoupSession *session;
    gboolean owns_session;

    /* WebSocket connection */
    SoupMessage *handshake_message;
    SoupWebsocketConnection *connection;
    GCancellable *cancellable;

    /* Signal handlers */
    gulong message_handler_id;
    gulong closed_handler_id;
    gulong error_handler_id;

    /* State */
    McpTransportState state;
    guint reconnect_attempts;
    guint reconnect_timeout_id;
    guint keepalive_timeout_id;

    /* Pending connect task */
    GTask *connect_task;
};

static void mcp_websocket_transport_iface_init (McpTransportInterface *iface);

G_DEFINE_TYPE_WITH_CODE (McpWebSocketTransport, mcp_websocket_transport, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MCP_TYPE_TRANSPORT,
                                                mcp_websocket_transport_iface_init))

enum
{
    PROP_0,
    PROP_URI,
    PROP_AUTH_TOKEN,
    PROP_PROTOCOLS,
    PROP_ORIGIN,
    PROP_RECONNECT_ENABLED,
    PROP_RECONNECT_DELAY,
    PROP_MAX_RECONNECT_ATTEMPTS,
    PROP_KEEPALIVE_INTERVAL,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

/* Forward declarations */
static void start_websocket_connection (McpWebSocketTransport *self);
static void stop_websocket_connection (McpWebSocketTransport *self);
static void schedule_reconnect (McpWebSocketTransport *self);

static void
set_state (McpWebSocketTransport *self,
           McpTransportState      new_state)
{
    McpTransportState old_state;

    if (self->state == new_state)
    {
        return;
    }

    old_state = self->state;
    self->state = new_state;

    mcp_transport_emit_state_changed (MCP_TRANSPORT (self), old_state, new_state);
}

/* Keepalive ping */

static gboolean
keepalive_timeout_cb (gpointer user_data)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (user_data);

    if (self->connection != NULL &&
        soup_websocket_connection_get_state (self->connection) == SOUP_WEBSOCKET_STATE_OPEN)
    {
        /* Send ping frame */
        soup_websocket_connection_send_text (self->connection, "");
    }

    return G_SOURCE_CONTINUE;
}

static void
start_keepalive (McpWebSocketTransport *self)
{
    if (self->keepalive_interval == 0)
    {
        return;
    }

    if (self->keepalive_timeout_id > 0)
    {
        return;
    }

    self->keepalive_timeout_id = g_timeout_add_seconds (self->keepalive_interval,
                                                         keepalive_timeout_cb,
                                                         self);
}

static void
stop_keepalive (McpWebSocketTransport *self)
{
    if (self->keepalive_timeout_id > 0)
    {
        g_source_remove (self->keepalive_timeout_id);
        self->keepalive_timeout_id = 0;
    }
}

/* WebSocket callbacks */

static void
on_websocket_message (SoupWebsocketConnection *connection,
                      SoupWebsocketDataType    type,
                      GBytes                  *message,
                      gpointer                 user_data)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (user_data);
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    const gchar *data;
    gsize length;
    JsonNode *root;

    if (type != SOUP_WEBSOCKET_DATA_TEXT)
    {
        /* Ignore binary messages */
        return;
    }

    data = g_bytes_get_data (message, &length);
    if (data == NULL || length == 0)
    {
        return;
    }

    /* Parse JSON */
    parser = json_parser_new ();
    if (!json_parser_load_from_data (parser, data, length, &error))
    {
        g_warning ("Failed to parse WebSocket message as JSON: %s", error->message);
        return;
    }

    root = json_parser_get_root (parser);
    if (root == NULL)
    {
        return;
    }

    /* Emit message-received signal */
    mcp_transport_emit_message_received (MCP_TRANSPORT (self), root);
}

static void
on_websocket_closed (SoupWebsocketConnection *connection,
                     gpointer                 user_data)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (user_data);

    stop_keepalive (self);

    if (self->reconnect_enabled && self->state == MCP_TRANSPORT_STATE_CONNECTED)
    {
        schedule_reconnect (self);
    }
    else
    {
        set_state (self, MCP_TRANSPORT_STATE_DISCONNECTED);
    }
}

static void
on_websocket_error (SoupWebsocketConnection *connection,
                    GError                  *error,
                    gpointer                 user_data)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (user_data);

    g_warning ("WebSocket error: %s", error->message);
    mcp_transport_emit_error (MCP_TRANSPORT (self), error);
}

static void
disconnect_websocket_signals (McpWebSocketTransport *self)
{
    if (self->connection == NULL)
    {
        return;
    }

    if (self->message_handler_id > 0)
    {
        g_signal_handler_disconnect (self->connection, self->message_handler_id);
        self->message_handler_id = 0;
    }

    if (self->closed_handler_id > 0)
    {
        g_signal_handler_disconnect (self->connection, self->closed_handler_id);
        self->closed_handler_id = 0;
    }

    if (self->error_handler_id > 0)
    {
        g_signal_handler_disconnect (self->connection, self->error_handler_id);
        self->error_handler_id = 0;
    }
}

static void
connect_websocket_signals (McpWebSocketTransport *self)
{
    self->message_handler_id = g_signal_connect (self->connection, "message",
                                                  G_CALLBACK (on_websocket_message),
                                                  self);
    self->closed_handler_id = g_signal_connect (self->connection, "closed",
                                                 G_CALLBACK (on_websocket_closed),
                                                 self);
    self->error_handler_id = g_signal_connect (self->connection, "error",
                                                G_CALLBACK (on_websocket_error),
                                                self);
}

static void
websocket_connect_cb (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (user_data);
    g_autoptr(GError) error = NULL;

    self->connection = soup_session_websocket_connect_finish (SOUP_SESSION (source),
                                                               result, &error);

    if (self->connection == NULL)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            g_warning ("WebSocket connection failed: %s", error->message);
            mcp_transport_emit_error (MCP_TRANSPORT (self), error);

            if (self->connect_task != NULL)
            {
                g_task_return_error (self->connect_task, g_steal_pointer (&error));
                g_clear_object (&self->connect_task);
            }

            if (self->reconnect_enabled)
            {
                schedule_reconnect (self);
            }
            else
            {
                set_state (self, MCP_TRANSPORT_STATE_ERROR);
            }
        }
        return;
    }

    /* Connection successful */
    connect_websocket_signals (self);
    self->reconnect_attempts = 0;

    set_state (self, MCP_TRANSPORT_STATE_CONNECTED);
    start_keepalive (self);

    if (self->connect_task != NULL)
    {
        g_task_return_boolean (self->connect_task, TRUE);
        g_clear_object (&self->connect_task);
    }
}

static void
start_websocket_connection (McpWebSocketTransport *self)
{
    SoupMessageHeaders *headers;

    self->handshake_message = soup_message_new ("GET", self->uri);
    if (self->handshake_message == NULL)
    {
        g_autoptr(GError) error = NULL;

        error = g_error_new (MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                              "Invalid WebSocket URI: %s", self->uri);
        mcp_transport_emit_error (MCP_TRANSPORT (self), error);

        if (self->connect_task != NULL)
        {
            g_task_return_error (self->connect_task, g_steal_pointer (&error));
            g_clear_object (&self->connect_task);
        }

        set_state (self, MCP_TRANSPORT_STATE_ERROR);
        return;
    }

    headers = soup_message_get_request_headers (self->handshake_message);

    /* Add auth header */
    if (self->auth_token != NULL && self->auth_token[0] != '\0')
    {
        g_autofree gchar *auth_value = NULL;
        auth_value = g_strdup_printf ("Bearer %s", self->auth_token);
        soup_message_headers_replace (headers, "Authorization", auth_value);
    }

    /* Add origin header */
    if (self->origin != NULL && self->origin[0] != '\0')
    {
        soup_message_headers_replace (headers, "Origin", self->origin);
    }

    self->cancellable = g_cancellable_new ();

    soup_session_websocket_connect_async (self->session,
                                           self->handshake_message,
                                           self->origin,
                                           (gchar **) self->protocols,
                                           G_PRIORITY_DEFAULT,
                                           self->cancellable,
                                           websocket_connect_cb,
                                           self);
}

static void
stop_websocket_connection (McpWebSocketTransport *self)
{
    stop_keepalive (self);

    if (self->cancellable != NULL)
    {
        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
    }

    if (self->connection != NULL)
    {
        disconnect_websocket_signals (self);

        if (soup_websocket_connection_get_state (self->connection) == SOUP_WEBSOCKET_STATE_OPEN)
        {
            soup_websocket_connection_close (self->connection,
                                              SOUP_WEBSOCKET_CLOSE_NORMAL,
                                              "Disconnecting");
        }

        g_clear_object (&self->connection);
    }

    g_clear_object (&self->handshake_message);

    if (self->reconnect_timeout_id > 0)
    {
        g_source_remove (self->reconnect_timeout_id);
        self->reconnect_timeout_id = 0;
    }
}

static gboolean
reconnect_timeout_cb (gpointer user_data)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (user_data);

    self->reconnect_timeout_id = 0;

    if (self->state == MCP_TRANSPORT_STATE_DISCONNECTED)
    {
        return G_SOURCE_REMOVE;
    }

    start_websocket_connection (self);

    return G_SOURCE_REMOVE;
}

static void
schedule_reconnect (McpWebSocketTransport *self)
{
    stop_websocket_connection (self);

    self->reconnect_attempts++;

    if (self->max_reconnect_attempts > 0 &&
        self->reconnect_attempts >= self->max_reconnect_attempts)
    {
        g_autoptr(GError) error = NULL;

        error = g_error_new (MCP_ERROR, MCP_ERROR_CONNECTION_CLOSED,
                              "Maximum reconnection attempts (%u) exceeded",
                              self->max_reconnect_attempts);
        mcp_transport_emit_error (MCP_TRANSPORT (self), error);
        set_state (self, MCP_TRANSPORT_STATE_ERROR);
        return;
    }

    set_state (self, MCP_TRANSPORT_STATE_CONNECTING);

    /* Exponential backoff: delay * 2^attempts, capped at 30 seconds */
    guint delay = self->reconnect_delay_ms * (1 << (self->reconnect_attempts - 1));
    if (delay > 30000)
    {
        delay = 30000;
    }

    self->reconnect_timeout_id = g_timeout_add (delay, reconnect_timeout_cb, self);
}

/* McpTransport interface implementation */

static McpTransportState
mcp_websocket_transport_get_state_impl (McpTransport *transport)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (transport);
    return self->state;
}

static void
mcp_websocket_transport_connect_async (McpTransport        *transport,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (transport);
    GTask *task;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_websocket_transport_connect_async);

    if (self->state != MCP_TRANSPORT_STATE_DISCONNECTED)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                  "Transport already connecting or connected");
        g_object_unref (task);
        return;
    }

    if (self->uri == NULL || self->uri[0] == '\0')
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                  "No URI set");
        g_object_unref (task);
        return;
    }

    self->connect_task = task;
    self->reconnect_attempts = 0;

    set_state (self, MCP_TRANSPORT_STATE_CONNECTING);
    start_websocket_connection (self);
}

static gboolean
mcp_websocket_transport_connect_finish (McpTransport  *transport,
                                         GAsyncResult  *result,
                                         GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_websocket_transport_disconnect_async (McpTransport        *transport,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (transport);
    GTask *task;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_websocket_transport_disconnect_async);

    set_state (self, MCP_TRANSPORT_STATE_DISCONNECTING);

    /* Disable reconnection for intentional disconnect */
    self->reconnect_enabled = FALSE;

    stop_websocket_connection (self);

    set_state (self, MCP_TRANSPORT_STATE_DISCONNECTED);

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static gboolean
mcp_websocket_transport_disconnect_finish (McpTransport  *transport,
                                            GAsyncResult  *result,
                                            GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_websocket_transport_send_message_async (McpTransport        *transport,
                                             JsonNode            *message,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (transport);
    GTask *task;
    g_autoptr(JsonGenerator) generator = NULL;
    g_autofree gchar *json_data = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_websocket_transport_send_message_async);

    if (self->connection == NULL ||
        soup_websocket_connection_get_state (self->connection) != SOUP_WEBSOCKET_STATE_OPEN)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                  "WebSocket not connected");
        g_object_unref (task);
        return;
    }

    /* Serialize JSON */
    generator = json_generator_new ();
    json_generator_set_root (generator, message);
    json_data = json_generator_to_data (generator, NULL);

    /* Send text message */
    soup_websocket_connection_send_text (self->connection, json_data);

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static gboolean
mcp_websocket_transport_send_message_finish (McpTransport  *transport,
                                              GAsyncResult  *result,
                                              GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_websocket_transport_iface_init (McpTransportInterface *iface)
{
    iface->get_state = mcp_websocket_transport_get_state_impl;
    iface->connect_async = mcp_websocket_transport_connect_async;
    iface->connect_finish = mcp_websocket_transport_connect_finish;
    iface->disconnect_async = mcp_websocket_transport_disconnect_async;
    iface->disconnect_finish = mcp_websocket_transport_disconnect_finish;
    iface->send_message_async = mcp_websocket_transport_send_message_async;
    iface->send_message_finish = mcp_websocket_transport_send_message_finish;
}

/* GObject implementation */

static void
mcp_websocket_transport_dispose (GObject *object)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (object);

    stop_websocket_connection (self);

    g_clear_object (&self->connect_task);

    if (self->owns_session)
    {
        g_clear_object (&self->session);
    }
    else
    {
        self->session = NULL;
    }

    G_OBJECT_CLASS (mcp_websocket_transport_parent_class)->dispose (object);
}

static void
mcp_websocket_transport_finalize (GObject *object)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (object);

    g_free (self->uri);
    g_free (self->auth_token);
    g_strfreev (self->protocols);
    g_free (self->origin);

    G_OBJECT_CLASS (mcp_websocket_transport_parent_class)->finalize (object);
}

static void
mcp_websocket_transport_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (object);

    switch (prop_id)
    {
        case PROP_URI:
            g_value_set_string (value, self->uri);
            break;
        case PROP_AUTH_TOKEN:
            g_value_set_string (value, self->auth_token);
            break;
        case PROP_PROTOCOLS:
            g_value_set_boxed (value, self->protocols);
            break;
        case PROP_ORIGIN:
            g_value_set_string (value, self->origin);
            break;
        case PROP_RECONNECT_ENABLED:
            g_value_set_boolean (value, self->reconnect_enabled);
            break;
        case PROP_RECONNECT_DELAY:
            g_value_set_uint (value, self->reconnect_delay_ms);
            break;
        case PROP_MAX_RECONNECT_ATTEMPTS:
            g_value_set_uint (value, self->max_reconnect_attempts);
            break;
        case PROP_KEEPALIVE_INTERVAL:
            g_value_set_uint (value, self->keepalive_interval);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_websocket_transport_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    McpWebSocketTransport *self = MCP_WEBSOCKET_TRANSPORT (object);

    switch (prop_id)
    {
        case PROP_URI:
            mcp_websocket_transport_set_uri (self, g_value_get_string (value));
            break;
        case PROP_AUTH_TOKEN:
            mcp_websocket_transport_set_auth_token (self, g_value_get_string (value));
            break;
        case PROP_PROTOCOLS:
            mcp_websocket_transport_set_protocols (self, g_value_get_boxed (value));
            break;
        case PROP_ORIGIN:
            mcp_websocket_transport_set_origin (self, g_value_get_string (value));
            break;
        case PROP_RECONNECT_ENABLED:
            mcp_websocket_transport_set_reconnect_enabled (self, g_value_get_boolean (value));
            break;
        case PROP_RECONNECT_DELAY:
            mcp_websocket_transport_set_reconnect_delay (self, g_value_get_uint (value));
            break;
        case PROP_MAX_RECONNECT_ATTEMPTS:
            mcp_websocket_transport_set_max_reconnect_attempts (self, g_value_get_uint (value));
            break;
        case PROP_KEEPALIVE_INTERVAL:
            mcp_websocket_transport_set_keepalive_interval (self, g_value_get_uint (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_websocket_transport_class_init (McpWebSocketTransportClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = mcp_websocket_transport_dispose;
    object_class->finalize = mcp_websocket_transport_finalize;
    object_class->get_property = mcp_websocket_transport_get_property;
    object_class->set_property = mcp_websocket_transport_set_property;

    properties[PROP_URI] =
        g_param_spec_string ("uri",
                             "URI",
                             "The WebSocket URI",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_AUTH_TOKEN] =
        g_param_spec_string ("auth-token",
                             "Auth Token",
                             "The authentication token",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_PROTOCOLS] =
        g_param_spec_boxed ("protocols",
                            "Protocols",
                            "The WebSocket subprotocols",
                            G_TYPE_STRV,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ORIGIN] =
        g_param_spec_string ("origin",
                             "Origin",
                             "The Origin header value",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RECONNECT_ENABLED] =
        g_param_spec_boolean ("reconnect-enabled",
                              "Reconnect Enabled",
                              "Whether automatic reconnection is enabled",
                              TRUE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RECONNECT_DELAY] =
        g_param_spec_uint ("reconnect-delay",
                           "Reconnect Delay",
                           "Initial reconnect delay in milliseconds",
                           0, G_MAXUINT, 1000,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MAX_RECONNECT_ATTEMPTS] =
        g_param_spec_uint ("max-reconnect-attempts",
                           "Max Reconnect Attempts",
                           "Maximum reconnection attempts (0 = unlimited)",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_KEEPALIVE_INTERVAL] =
        g_param_spec_uint ("keepalive-interval",
                           "Keepalive Interval",
                           "Keepalive ping interval in seconds (0 = disabled)",
                           0, G_MAXUINT, 30,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
mcp_websocket_transport_init (McpWebSocketTransport *self)
{
    self->state = MCP_TRANSPORT_STATE_DISCONNECTED;
    self->reconnect_enabled = TRUE;
    self->reconnect_delay_ms = 1000;
    self->max_reconnect_attempts = 0;
    self->keepalive_interval = 30;
}

/* Public API */

McpWebSocketTransport *
mcp_websocket_transport_new (const gchar *uri)
{
    McpWebSocketTransport *self;

    g_return_val_if_fail (uri != NULL, NULL);

    self = g_object_new (MCP_TYPE_WEBSOCKET_TRANSPORT, NULL);
    self->uri = g_strdup (uri);
    self->session = soup_session_new ();
    self->owns_session = TRUE;

    return self;
}

McpWebSocketTransport *
mcp_websocket_transport_new_with_session (const gchar *uri,
                                           SoupSession *session)
{
    McpWebSocketTransport *self;

    g_return_val_if_fail (uri != NULL, NULL);
    g_return_val_if_fail (SOUP_IS_SESSION (session), NULL);

    self = g_object_new (MCP_TYPE_WEBSOCKET_TRANSPORT, NULL);
    self->uri = g_strdup (uri);
    self->session = session;
    self->owns_session = FALSE;

    return self;
}

const gchar *
mcp_websocket_transport_get_uri (McpWebSocketTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self), NULL);
    return self->uri;
}

void
mcp_websocket_transport_set_uri (McpWebSocketTransport *self,
                                  const gchar           *uri)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self));
    g_return_if_fail (self->state == MCP_TRANSPORT_STATE_DISCONNECTED);

    g_free (self->uri);
    self->uri = g_strdup (uri);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_URI]);
}

const gchar *
mcp_websocket_transport_get_auth_token (McpWebSocketTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self), NULL);
    return self->auth_token;
}

void
mcp_websocket_transport_set_auth_token (McpWebSocketTransport *self,
                                         const gchar           *token)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self));

    g_free (self->auth_token);
    self->auth_token = g_strdup (token);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTH_TOKEN]);
}

const gchar * const *
mcp_websocket_transport_get_protocols (McpWebSocketTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self), NULL);
    return (const gchar * const *) self->protocols;
}

void
mcp_websocket_transport_set_protocols (McpWebSocketTransport *self,
                                        const gchar * const   *protocols)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self));

    g_strfreev (self->protocols);
    self->protocols = g_strdupv ((gchar **) protocols);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROTOCOLS]);
}

const gchar *
mcp_websocket_transport_get_origin (McpWebSocketTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self), NULL);
    return self->origin;
}

void
mcp_websocket_transport_set_origin (McpWebSocketTransport *self,
                                     const gchar           *origin)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self));

    g_free (self->origin);
    self->origin = g_strdup (origin);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ORIGIN]);
}

SoupSession *
mcp_websocket_transport_get_soup_session (McpWebSocketTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self), NULL);
    return self->session;
}

SoupWebsocketConnection *
mcp_websocket_transport_get_websocket_connection (McpWebSocketTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self), NULL);
    return self->connection;
}

void
mcp_websocket_transport_set_reconnect_enabled (McpWebSocketTransport *self,
                                                gboolean               enabled)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self));

    self->reconnect_enabled = enabled;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RECONNECT_ENABLED]);
}

gboolean
mcp_websocket_transport_get_reconnect_enabled (McpWebSocketTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self), FALSE);
    return self->reconnect_enabled;
}

void
mcp_websocket_transport_set_reconnect_delay (McpWebSocketTransport *self,
                                              guint                  delay_ms)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self));

    self->reconnect_delay_ms = delay_ms;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RECONNECT_DELAY]);
}

guint
mcp_websocket_transport_get_reconnect_delay (McpWebSocketTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self), 0);
    return self->reconnect_delay_ms;
}

void
mcp_websocket_transport_set_max_reconnect_attempts (McpWebSocketTransport *self,
                                                     guint                  max_attempts)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self));

    self->max_reconnect_attempts = max_attempts;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_RECONNECT_ATTEMPTS]);
}

guint
mcp_websocket_transport_get_max_reconnect_attempts (McpWebSocketTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self), 0);
    return self->max_reconnect_attempts;
}

void
mcp_websocket_transport_set_keepalive_interval (McpWebSocketTransport *self,
                                                 guint                  interval_seconds)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self));

    self->keepalive_interval = interval_seconds;

    /* Restart keepalive if connected */
    if (self->state == MCP_TRANSPORT_STATE_CONNECTED)
    {
        stop_keepalive (self);
        start_keepalive (self);
    }

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEEPALIVE_INTERVAL]);
}

guint
mcp_websocket_transport_get_keepalive_interval (McpWebSocketTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_TRANSPORT (self), 0);
    return self->keepalive_interval;
}
