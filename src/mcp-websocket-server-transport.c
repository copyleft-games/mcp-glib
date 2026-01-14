/*
 * mcp-websocket-server-transport.c - WebSocket server transport implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp-websocket-server-transport.h"
#include "mcp-error.h"
#undef MCP_COMPILATION

#include <string.h>

/**
 * SECTION:mcp-websocket-server-transport
 * @title: McpWebSocketServerTransport
 * @short_description: WebSocket server transport implementation
 *
 * #McpWebSocketServerTransport provides a WebSocket server-based transport for
 * MCP communication. It listens for incoming WebSocket connections and provides
 * bidirectional communication via text frames.
 *
 * This is the server-side counterpart to #McpWebSocketTransport.
 */

struct _McpWebSocketServerTransport
{
    GObject parent_instance;

    /* Configuration */
    gchar *host;
    guint  port;
    gchar *path;
    gchar **protocols;
    gchar *origin;
    gboolean require_auth;
    gchar *auth_token;
    guint  keepalive_interval;
    GTlsCertificate *tls_certificate;

    /* Server */
    SoupServer *server;
    guint actual_port;

    /* Client state (single client model) */
    SoupWebsocketConnection *connection;
    gboolean client_connected;
    gulong message_handler_id;
    gulong closed_handler_id;
    gulong error_handler_id;

    /* Keepalive */
    guint keepalive_timeout_id;

    /* Transport state */
    McpTransportState state;
};

static void mcp_websocket_server_transport_iface_init (McpTransportInterface *iface);

G_DEFINE_TYPE_WITH_CODE (McpWebSocketServerTransport, mcp_websocket_server_transport, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MCP_TYPE_TRANSPORT,
                                                mcp_websocket_server_transport_iface_init))

enum
{
    PROP_0,
    PROP_PORT,
    PROP_HOST,
    PROP_PATH,
    PROP_PROTOCOLS,
    PROP_ORIGIN,
    PROP_REQUIRE_AUTH,
    PROP_AUTH_TOKEN,
    PROP_KEEPALIVE_INTERVAL,
    PROP_TLS_CERTIFICATE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

/* Forward declarations */
static void stop_server (McpWebSocketServerTransport *self);
static void start_keepalive (McpWebSocketServerTransport *self);
static void stop_keepalive (McpWebSocketServerTransport *self);

static void
set_state (McpWebSocketServerTransport *self,
           McpTransportState            new_state)
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

/*
 * Validate authentication token from request headers
 */
static gboolean
validate_auth (McpWebSocketServerTransport *self,
               SoupServerMessage           *msg)
{
    SoupMessageHeaders *headers;
    const gchar *auth_header;
    const gchar *token;

    if (!self->require_auth)
    {
        return TRUE;
    }

    headers = soup_server_message_get_request_headers (msg);
    auth_header = soup_message_headers_get_one (headers, "Authorization");

    if (auth_header == NULL)
    {
        return FALSE;
    }

    /* Check for "Bearer " prefix */
    if (!g_str_has_prefix (auth_header, "Bearer "))
    {
        return FALSE;
    }

    token = auth_header + 7; /* Skip "Bearer " */

    if (self->auth_token == NULL)
    {
        return FALSE;
    }

    return g_strcmp0 (token, self->auth_token) == 0;
}

/*
 * Validate Origin header if required
 */
static gboolean
validate_origin (McpWebSocketServerTransport *self,
                 SoupServerMessage           *msg)
{
    SoupMessageHeaders *headers;
    const gchar *origin_header;

    if (self->origin == NULL || self->origin[0] == '\0')
    {
        return TRUE; /* No origin requirement */
    }

    headers = soup_server_message_get_request_headers (msg);
    origin_header = soup_message_headers_get_one (headers, "Origin");

    if (origin_header == NULL)
    {
        return FALSE;
    }

    return g_strcmp0 (origin_header, self->origin) == 0;
}

/*
 * Handle incoming WebSocket message
 */
static void
on_websocket_message (SoupWebsocketConnection *connection,
                      SoupWebsocketDataType    type,
                      GBytes                  *message,
                      gpointer                 user_data)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (user_data);
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    const gchar *data;
    gsize len;
    JsonNode *root;

    /* Only handle text messages */
    if (type != SOUP_WEBSOCKET_DATA_TEXT)
    {
        return;
    }

    data = g_bytes_get_data (message, &len);
    if (data == NULL || len == 0)
    {
        return;
    }

    /* Parse JSON */
    parser = json_parser_new ();
    if (!json_parser_load_from_data (parser, data, len, &error))
    {
        g_autoptr(GError) parse_error = NULL;

        parse_error = g_error_new (MCP_ERROR, MCP_ERROR_PARSE_ERROR,
                                    "Invalid JSON: %s", error->message);
        mcp_transport_emit_error (MCP_TRANSPORT (self), parse_error);
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

/*
 * Handle WebSocket connection closed
 */
static void
on_websocket_closed (SoupWebsocketConnection *connection,
                     gpointer                 user_data)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (user_data);

    stop_keepalive (self);

    /* Disconnect signal handlers */
    if (self->message_handler_id > 0)
    {
        g_signal_handler_disconnect (connection, self->message_handler_id);
        self->message_handler_id = 0;
    }
    if (self->closed_handler_id > 0)
    {
        g_signal_handler_disconnect (connection, self->closed_handler_id);
        self->closed_handler_id = 0;
    }
    if (self->error_handler_id > 0)
    {
        g_signal_handler_disconnect (connection, self->error_handler_id);
        self->error_handler_id = 0;
    }

    g_clear_object (&self->connection);
    self->client_connected = FALSE;
}

/*
 * Handle WebSocket error
 */
static void
on_websocket_error (SoupWebsocketConnection *connection,
                    GError                  *error,
                    gpointer                 user_data)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (user_data);

    mcp_transport_emit_error (MCP_TRANSPORT (self), error);
}

/*
 * Handle incoming WebSocket connection
 */
static void
websocket_handler (SoupServer              *server,
                   SoupServerMessage       *msg,
                   const char              *path,
                   SoupWebsocketConnection *connection,
                   gpointer                 user_data)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (user_data);

    /* Only allow one client at a time */
    if (self->client_connected)
    {
        soup_websocket_connection_close (connection, SOUP_WEBSOCKET_CLOSE_GOING_AWAY,
                                          "Server busy");
        return;
    }

    /* Store connection */
    self->connection = g_object_ref (connection);
    self->client_connected = TRUE;

    /* Connect signal handlers */
    self->message_handler_id = g_signal_connect (connection, "message",
                                                   G_CALLBACK (on_websocket_message), self);
    self->closed_handler_id = g_signal_connect (connection, "closed",
                                                  G_CALLBACK (on_websocket_closed), self);
    self->error_handler_id = g_signal_connect (connection, "error",
                                                 G_CALLBACK (on_websocket_error), self);

    /* Start keepalive if configured */
    start_keepalive (self);
}

/*
 * Callback for WebSocket authentication validation
 *
 * This is an early handler that runs before the WebSocket upgrade.
 * If authentication fails, we set the status and the request won't proceed.
 */
static void
websocket_auth_callback (SoupServer        *server,
                         SoupServerMessage *msg,
                         const char        *path,
                         GHashTable        *query,
                         gpointer           user_data)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (user_data);

    /* Validate authentication */
    if (!validate_auth (self, msg))
    {
        soup_server_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED, NULL);
        return;
    }

    /* Validate origin */
    if (!validate_origin (self, msg))
    {
        soup_server_message_set_status (msg, SOUP_STATUS_FORBIDDEN, NULL);
        return;
    }

    /* Continue to WebSocket handler */
}

/*
 * Keepalive ping callback
 */
static gboolean
keepalive_timeout_cb (gpointer user_data)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (user_data);

    if (self->connection == NULL ||
        soup_websocket_connection_get_state (self->connection) != SOUP_WEBSOCKET_STATE_OPEN)
    {
        self->keepalive_timeout_id = 0;
        return G_SOURCE_REMOVE;
    }

    /* Send empty text frame as keepalive (matches client implementation) */
    soup_websocket_connection_send_text (self->connection, "");

    return G_SOURCE_CONTINUE;
}

static void
start_keepalive (McpWebSocketServerTransport *self)
{
    if (self->keepalive_interval == 0)
    {
        return;
    }

    if (self->keepalive_timeout_id > 0)
    {
        return; /* Already running */
    }

    self->keepalive_timeout_id = g_timeout_add_seconds (self->keepalive_interval,
                                                         keepalive_timeout_cb,
                                                         self);
}

static void
stop_keepalive (McpWebSocketServerTransport *self)
{
    if (self->keepalive_timeout_id > 0)
    {
        g_source_remove (self->keepalive_timeout_id);
        self->keepalive_timeout_id = 0;
    }
}

/* McpTransport interface implementation */

static McpTransportState
mcp_websocket_server_transport_get_state_impl (McpTransport *transport)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (transport);
    return self->state;
}

static void
mcp_websocket_server_transport_connect_async (McpTransport        *transport,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;
    g_autoptr(GError) error = NULL;
    GSList *uris;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_websocket_server_transport_connect_async);

    if (self->state != MCP_TRANSPORT_STATE_DISCONNECTED)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                  "Transport already connecting or connected");
        return;
    }

    set_state (self, MCP_TRANSPORT_STATE_CONNECTING);

    /* Create server */
    self->server = soup_server_new (NULL, NULL);

    /* Set TLS certificate if provided */
    if (self->tls_certificate != NULL)
    {
        soup_server_set_tls_certificate (self->server, self->tls_certificate);
    }

    /* Add WebSocket handler */
    soup_server_add_websocket_handler (self->server,
                                        self->path,
                                        self->origin,  /* Required origin (NULL = any) */
                                        self->protocols,
                                        websocket_handler,
                                        self,
                                        NULL);

    /* Add early handler for auth validation */
    if (self->require_auth || self->origin != NULL)
    {
        soup_server_add_early_handler (self->server,
                                        self->path,
                                        websocket_auth_callback,
                                        self,
                                        NULL);
    }

    /* Start listening */
    if (self->host != NULL && self->host[0] != '\0')
    {
        if (g_strcmp0 (self->host, "127.0.0.1") == 0 ||
            g_strcmp0 (self->host, "localhost") == 0)
        {
            if (!soup_server_listen_local (self->server, self->port, 0, &error))
            {
                g_task_return_error (task, g_steal_pointer (&error));
                set_state (self, MCP_TRANSPORT_STATE_ERROR);
                return;
            }
        }
        else
        {
            if (!soup_server_listen_all (self->server, self->port, 0, &error))
            {
                g_task_return_error (task, g_steal_pointer (&error));
                set_state (self, MCP_TRANSPORT_STATE_ERROR);
                return;
            }
        }
    }
    else
    {
        if (!soup_server_listen_all (self->server, self->port, 0, &error))
        {
            g_task_return_error (task, g_steal_pointer (&error));
            set_state (self, MCP_TRANSPORT_STATE_ERROR);
            return;
        }
    }

    /* Get the actual port */
    uris = soup_server_get_uris (self->server);
    if (uris != NULL)
    {
        GUri *uri = uris->data;
        self->actual_port = g_uri_get_port (uri);
        g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);
    }
    else
    {
        self->actual_port = self->port;
    }

    set_state (self, MCP_TRANSPORT_STATE_CONNECTED);

    g_task_return_boolean (task, TRUE);
}

static gboolean
mcp_websocket_server_transport_connect_finish (McpTransport  *transport,
                                                GAsyncResult  *result,
                                                GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_websocket_server_transport_disconnect_async (McpTransport        *transport,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_websocket_server_transport_disconnect_async);

    set_state (self, MCP_TRANSPORT_STATE_DISCONNECTING);

    stop_server (self);

    set_state (self, MCP_TRANSPORT_STATE_DISCONNECTED);

    g_task_return_boolean (task, TRUE);
}

static gboolean
mcp_websocket_server_transport_disconnect_finish (McpTransport  *transport,
                                                   GAsyncResult  *result,
                                                   GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_websocket_server_transport_send_message_async (McpTransport        *transport,
                                                    JsonNode            *message,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;
    g_autoptr(JsonGenerator) generator = NULL;
    g_autofree gchar *json_data = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_websocket_server_transport_send_message_async);

    if (self->state != MCP_TRANSPORT_STATE_CONNECTED)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                  "Transport not connected");
        return;
    }

    if (!self->client_connected || self->connection == NULL)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                  "No client connected");
        return;
    }

    if (soup_websocket_connection_get_state (self->connection) != SOUP_WEBSOCKET_STATE_OPEN)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                  "WebSocket connection not open");
        return;
    }

    /* Serialize JSON */
    generator = json_generator_new ();
    json_generator_set_root (generator, message);
    json_data = json_generator_to_data (generator, NULL);

    /* Send as text frame */
    soup_websocket_connection_send_text (self->connection, json_data);

    g_task_return_boolean (task, TRUE);
}

static gboolean
mcp_websocket_server_transport_send_message_finish (McpTransport  *transport,
                                                     GAsyncResult  *result,
                                                     GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_websocket_server_transport_iface_init (McpTransportInterface *iface)
{
    iface->get_state = mcp_websocket_server_transport_get_state_impl;
    iface->connect_async = mcp_websocket_server_transport_connect_async;
    iface->connect_finish = mcp_websocket_server_transport_connect_finish;
    iface->disconnect_async = mcp_websocket_server_transport_disconnect_async;
    iface->disconnect_finish = mcp_websocket_server_transport_disconnect_finish;
    iface->send_message_async = mcp_websocket_server_transport_send_message_async;
    iface->send_message_finish = mcp_websocket_server_transport_send_message_finish;
}

static void
stop_server (McpWebSocketServerTransport *self)
{
    stop_keepalive (self);

    /* Close WebSocket connection */
    if (self->connection != NULL)
    {
        /* Disconnect signal handlers */
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

        if (soup_websocket_connection_get_state (self->connection) == SOUP_WEBSOCKET_STATE_OPEN)
        {
            soup_websocket_connection_close (self->connection, SOUP_WEBSOCKET_CLOSE_NORMAL, "Server shutdown");
        }

        g_clear_object (&self->connection);
        self->client_connected = FALSE;
    }

    /* Stop server */
    if (self->server != NULL)
    {
        soup_server_disconnect (self->server);
        g_clear_object (&self->server);
    }

    self->actual_port = 0;
}

/* GObject implementation */

static void
mcp_websocket_server_transport_dispose (GObject *object)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (object);

    stop_server (self);
    g_clear_object (&self->tls_certificate);

    G_OBJECT_CLASS (mcp_websocket_server_transport_parent_class)->dispose (object);
}

static void
mcp_websocket_server_transport_finalize (GObject *object)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (object);

    g_free (self->host);
    g_free (self->path);
    g_strfreev (self->protocols);
    g_free (self->origin);
    g_free (self->auth_token);

    G_OBJECT_CLASS (mcp_websocket_server_transport_parent_class)->finalize (object);
}

static void
mcp_websocket_server_transport_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (object);

    switch (prop_id)
    {
        case PROP_PORT:
            g_value_set_uint (value, self->port);
            break;
        case PROP_HOST:
            g_value_set_string (value, self->host);
            break;
        case PROP_PATH:
            g_value_set_string (value, self->path);
            break;
        case PROP_PROTOCOLS:
            g_value_set_boxed (value, self->protocols);
            break;
        case PROP_ORIGIN:
            g_value_set_string (value, self->origin);
            break;
        case PROP_REQUIRE_AUTH:
            g_value_set_boolean (value, self->require_auth);
            break;
        case PROP_AUTH_TOKEN:
            g_value_set_string (value, self->auth_token);
            break;
        case PROP_KEEPALIVE_INTERVAL:
            g_value_set_uint (value, self->keepalive_interval);
            break;
        case PROP_TLS_CERTIFICATE:
            g_value_set_object (value, self->tls_certificate);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_websocket_server_transport_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
    McpWebSocketServerTransport *self = MCP_WEBSOCKET_SERVER_TRANSPORT (object);

    switch (prop_id)
    {
        case PROP_PORT:
            mcp_websocket_server_transport_set_port (self, g_value_get_uint (value));
            break;
        case PROP_HOST:
            mcp_websocket_server_transport_set_host (self, g_value_get_string (value));
            break;
        case PROP_PATH:
            mcp_websocket_server_transport_set_path (self, g_value_get_string (value));
            break;
        case PROP_PROTOCOLS:
            mcp_websocket_server_transport_set_protocols (self, g_value_get_boxed (value));
            break;
        case PROP_ORIGIN:
            mcp_websocket_server_transport_set_origin (self, g_value_get_string (value));
            break;
        case PROP_REQUIRE_AUTH:
            mcp_websocket_server_transport_set_require_auth (self, g_value_get_boolean (value));
            break;
        case PROP_AUTH_TOKEN:
            mcp_websocket_server_transport_set_auth_token (self, g_value_get_string (value));
            break;
        case PROP_KEEPALIVE_INTERVAL:
            mcp_websocket_server_transport_set_keepalive_interval (self, g_value_get_uint (value));
            break;
        case PROP_TLS_CERTIFICATE:
            mcp_websocket_server_transport_set_tls_certificate (self, g_value_get_object (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_websocket_server_transport_class_init (McpWebSocketServerTransportClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = mcp_websocket_server_transport_dispose;
    object_class->finalize = mcp_websocket_server_transport_finalize;
    object_class->get_property = mcp_websocket_server_transport_get_property;
    object_class->set_property = mcp_websocket_server_transport_set_property;

    /**
     * McpWebSocketServerTransport:port:
     *
     * The port to listen on. Use 0 for auto-assignment.
     */
    properties[PROP_PORT] =
        g_param_spec_uint ("port",
                           "Port",
                           "The port to listen on",
                           0, G_MAXUINT16, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpWebSocketServerTransport:host:
     *
     * The host/address to bind to. NULL means all interfaces.
     */
    properties[PROP_HOST] =
        g_param_spec_string ("host",
                             "Host",
                             "The host/address to bind to",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpWebSocketServerTransport:path:
     *
     * The WebSocket endpoint path.
     */
    properties[PROP_PATH] =
        g_param_spec_string ("path",
                             "Path",
                             "The WebSocket endpoint path",
                             "/",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpWebSocketServerTransport:protocols:
     *
     * The accepted WebSocket subprotocols.
     */
    properties[PROP_PROTOCOLS] =
        g_param_spec_boxed ("protocols",
                            "Protocols",
                            "The accepted WebSocket subprotocols",
                            G_TYPE_STRV,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpWebSocketServerTransport:origin:
     *
     * The required Origin header value. NULL accepts any origin.
     */
    properties[PROP_ORIGIN] =
        g_param_spec_string ("origin",
                             "Origin",
                             "The required Origin header value",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpWebSocketServerTransport:require-auth:
     *
     * Whether to require Bearer token authentication.
     */
    properties[PROP_REQUIRE_AUTH] =
        g_param_spec_boolean ("require-auth",
                              "Require Auth",
                              "Whether to require authentication",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpWebSocketServerTransport:auth-token:
     *
     * The expected Bearer token for authentication.
     */
    properties[PROP_AUTH_TOKEN] =
        g_param_spec_string ("auth-token",
                             "Auth Token",
                             "The expected Bearer token",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpWebSocketServerTransport:keepalive-interval:
     *
     * The keepalive ping interval in seconds. 0 disables keepalive.
     */
    properties[PROP_KEEPALIVE_INTERVAL] =
        g_param_spec_uint ("keepalive-interval",
                           "Keepalive Interval",
                           "The keepalive ping interval in seconds",
                           0, G_MAXUINT, 30,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpWebSocketServerTransport:tls-certificate:
     *
     * The TLS certificate for WSS.
     */
    properties[PROP_TLS_CERTIFICATE] =
        g_param_spec_object ("tls-certificate",
                             "TLS Certificate",
                             "The TLS certificate for WSS",
                             G_TYPE_TLS_CERTIFICATE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
mcp_websocket_server_transport_init (McpWebSocketServerTransport *self)
{
    self->state = MCP_TRANSPORT_STATE_DISCONNECTED;
    self->path = g_strdup ("/");
    self->require_auth = FALSE;
    self->keepalive_interval = 30;
    self->client_connected = FALSE;
}

/* Public API */

/**
 * mcp_websocket_server_transport_new:
 * @port: the port to listen on (0 = auto-assign)
 *
 * Creates a new WebSocket server transport.
 *
 * Returns: (transfer full): a new #McpWebSocketServerTransport
 */
McpWebSocketServerTransport *
mcp_websocket_server_transport_new (guint port)
{
    McpWebSocketServerTransport *self;

    self = g_object_new (MCP_TYPE_WEBSOCKET_SERVER_TRANSPORT, NULL);
    self->port = port;

    return self;
}

/**
 * mcp_websocket_server_transport_new_full:
 * @host: (nullable): the host/address to bind to
 * @port: the port to listen on
 * @path: (nullable): the WebSocket endpoint path
 *
 * Creates a new WebSocket server transport with specific configuration.
 *
 * Returns: (transfer full): a new #McpWebSocketServerTransport
 */
McpWebSocketServerTransport *
mcp_websocket_server_transport_new_full (const gchar *host,
                                          guint        port,
                                          const gchar *path)
{
    McpWebSocketServerTransport *self;

    self = g_object_new (MCP_TYPE_WEBSOCKET_SERVER_TRANSPORT, NULL);
    self->host = g_strdup (host);
    self->port = port;
    if (path != NULL)
    {
        g_free (self->path);
        self->path = g_strdup (path);
    }

    return self;
}

guint
mcp_websocket_server_transport_get_port (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), 0);
    return self->port;
}

void
mcp_websocket_server_transport_set_port (McpWebSocketServerTransport *self,
                                          guint                        port)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self));
    g_return_if_fail (self->state == MCP_TRANSPORT_STATE_DISCONNECTED);

    self->port = port;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PORT]);
}

const gchar *
mcp_websocket_server_transport_get_host (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), NULL);
    return self->host;
}

void
mcp_websocket_server_transport_set_host (McpWebSocketServerTransport *self,
                                          const gchar                  *host)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self));
    g_return_if_fail (self->state == MCP_TRANSPORT_STATE_DISCONNECTED);

    g_free (self->host);
    self->host = g_strdup (host);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HOST]);
}

const gchar *
mcp_websocket_server_transport_get_path (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), NULL);
    return self->path;
}

void
mcp_websocket_server_transport_set_path (McpWebSocketServerTransport *self,
                                          const gchar                  *path)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self));

    g_free (self->path);
    self->path = g_strdup (path != NULL ? path : "/");
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
}

const gchar * const *
mcp_websocket_server_transport_get_protocols (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), NULL);
    return (const gchar * const *) self->protocols;
}

void
mcp_websocket_server_transport_set_protocols (McpWebSocketServerTransport *self,
                                               const gchar * const          *protocols)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self));

    g_strfreev (self->protocols);
    self->protocols = g_strdupv ((gchar **) protocols);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROTOCOLS]);
}

const gchar *
mcp_websocket_server_transport_get_origin (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), NULL);
    return self->origin;
}

void
mcp_websocket_server_transport_set_origin (McpWebSocketServerTransport *self,
                                            const gchar                  *origin)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self));

    g_free (self->origin);
    self->origin = g_strdup (origin);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ORIGIN]);
}

gboolean
mcp_websocket_server_transport_get_require_auth (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), FALSE);
    return self->require_auth;
}

void
mcp_websocket_server_transport_set_require_auth (McpWebSocketServerTransport *self,
                                                  gboolean                      require_auth)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self));

    self->require_auth = require_auth;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REQUIRE_AUTH]);
}

const gchar *
mcp_websocket_server_transport_get_auth_token (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), NULL);
    return self->auth_token;
}

void
mcp_websocket_server_transport_set_auth_token (McpWebSocketServerTransport *self,
                                                const gchar                  *token)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self));

    g_free (self->auth_token);
    self->auth_token = g_strdup (token);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTH_TOKEN]);
}

guint
mcp_websocket_server_transport_get_keepalive_interval (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), 0);
    return self->keepalive_interval;
}

void
mcp_websocket_server_transport_set_keepalive_interval (McpWebSocketServerTransport *self,
                                                        guint                        interval_seconds)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self));

    self->keepalive_interval = interval_seconds;

    /* Restart keepalive if currently running */
    if (self->client_connected)
    {
        stop_keepalive (self);
        if (interval_seconds > 0)
        {
            start_keepalive (self);
        }
    }

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEEPALIVE_INTERVAL]);
}

GTlsCertificate *
mcp_websocket_server_transport_get_tls_certificate (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), NULL);
    return self->tls_certificate;
}

void
mcp_websocket_server_transport_set_tls_certificate (McpWebSocketServerTransport *self,
                                                     GTlsCertificate              *certificate)
{
    g_return_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self));
    g_return_if_fail (self->state == MCP_TRANSPORT_STATE_DISCONNECTED);

    g_set_object (&self->tls_certificate, certificate);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TLS_CERTIFICATE]);
}

SoupServer *
mcp_websocket_server_transport_get_soup_server (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), NULL);
    return self->server;
}

SoupWebsocketConnection *
mcp_websocket_server_transport_get_websocket_connection (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), NULL);
    return self->connection;
}

gboolean
mcp_websocket_server_transport_has_client (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), FALSE);
    return self->client_connected;
}

guint
mcp_websocket_server_transport_get_actual_port (McpWebSocketServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_WEBSOCKET_SERVER_TRANSPORT (self), 0);
    return self->actual_port;
}
