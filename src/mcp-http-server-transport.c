/*
 * mcp-http-server-transport.c - HTTP server transport implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp-http-server-transport.h"
#include "mcp-error.h"
#undef MCP_COMPILATION

#include <string.h>

/**
 * SECTION:mcp-http-server-transport
 * @title: McpHttpServerTransport
 * @short_description: HTTP server transport implementation
 *
 * #McpHttpServerTransport provides an HTTP server-based transport for MCP
 * communication. It listens for incoming connections and uses:
 * - HTTP POST requests to receive messages from clients
 * - Server-Sent Events (SSE) to send messages to clients
 *
 * This is the server-side counterpart to #McpHttpTransport.
 */

struct _McpHttpServerTransport
{
    GObject parent_instance;

    /* Configuration */
    gchar *host;
    guint  port;
    gchar *post_path;
    gchar *sse_path;
    gboolean require_auth;
    gchar *auth_token;
    GTlsCertificate *tls_certificate;

    /* Server */
    SoupServer *server;
    guint actual_port;

    /* Client state (single client model) */
    gchar *session_id;
    SoupServerMessage *sse_message;
    gboolean client_connected;
    guint event_id_counter;

    /* Transport state */
    McpTransportState state;

    /* Streamable HTTP: pending POST response for inline reply */
    SoupServerMessage *pending_post_msg;
};

static void mcp_http_server_transport_iface_init (McpTransportInterface *iface);

G_DEFINE_TYPE_WITH_CODE (McpHttpServerTransport, mcp_http_server_transport, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MCP_TYPE_TRANSPORT,
                                                mcp_http_server_transport_iface_init))

enum
{
    PROP_0,
    PROP_PORT,
    PROP_HOST,
    PROP_POST_PATH,
    PROP_SSE_PATH,
    PROP_SESSION_ID,
    PROP_REQUIRE_AUTH,
    PROP_AUTH_TOKEN,
    PROP_TLS_CERTIFICATE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

/* Forward declarations */
static void stop_server (McpHttpServerTransport *self);

static void
set_state (McpHttpServerTransport *self,
           McpTransportState       new_state)
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
 * Generate a UUID for session ID
 */
static gchar *
generate_session_id (void)
{
    g_autofree gchar *uuid = NULL;
    uuid = g_uuid_string_random ();
    return g_steal_pointer (&uuid);
}

/*
 * Validate authentication token from request headers
 */
static gboolean
validate_auth (McpHttpServerTransport *self,
               SoupServerMessage      *msg)
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
 * Send an SSE event to the connected client
 */
static gboolean
send_sse_event (McpHttpServerTransport *self,
                const gchar            *event_type,
                const gchar            *data)
{
    SoupMessageBody *body;
    g_autofree gchar *event_str = NULL;
    g_autofree gchar *event_id_str = NULL;

    if (self->sse_message == NULL || !self->client_connected)
    {
        return FALSE;
    }

    self->event_id_counter++;
    event_id_str = g_strdup_printf ("%u", self->event_id_counter);

    /* Build SSE event string */
    if (event_type != NULL && event_type[0] != '\0')
    {
        event_str = g_strdup_printf ("id: %s\nevent: %s\ndata: %s\n\n",
                                      event_id_str, event_type, data);
    }
    else
    {
        event_str = g_strdup_printf ("id: %s\ndata: %s\n\n",
                                      event_id_str, data);
    }

    body = soup_server_message_get_response_body (self->sse_message);
    soup_message_body_append (body, SOUP_MEMORY_COPY, event_str, strlen (event_str));

    /* Trigger sending the chunk */
    soup_server_message_unpause (self->sse_message);

    return TRUE;
}

/*
 * Handle SSE connection (GET request to sse_path)
 */
static void
handle_sse_request (SoupServer        *server,
                    SoupServerMessage *msg,
                    const char        *path,
                    GHashTable        *query,
                    gpointer           user_data)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (user_data);
    SoupMessageHeaders *response_headers;
    SoupMessageHeaders *request_headers;
    const gchar *accept_header;

    /* Validate authentication */
    if (!validate_auth (self, msg))
    {
        soup_server_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED, NULL);
        return;
    }

    /* Check Accept header */
    request_headers = soup_server_message_get_request_headers (msg);
    accept_header = soup_message_headers_get_one (request_headers, "Accept");
    if (accept_header == NULL || !g_str_has_prefix (accept_header, "text/event-stream"))
    {
        soup_server_message_set_status (msg, SOUP_STATUS_NOT_ACCEPTABLE, NULL);
        return;
    }

    /* Only allow one client at a time */
    if (self->client_connected)
    {
        soup_server_message_set_status (msg, SOUP_STATUS_CONFLICT, NULL);
        return;
    }

    /* Generate session ID */
    g_free (self->session_id);
    self->session_id = generate_session_id ();

    /* Set response headers */
    response_headers = soup_server_message_get_response_headers (msg);
    soup_message_headers_replace (response_headers, "Content-Type", "text/event-stream");
    soup_message_headers_replace (response_headers, "Cache-Control", "no-cache");
    soup_message_headers_replace (response_headers, "Connection", "keep-alive");
    soup_message_headers_replace (response_headers, "Mcp-Session-Id", self->session_id);
    soup_message_headers_replace (response_headers, "X-Accel-Buffering", "no");

    /* Set status and enable chunked encoding */
    soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);

    /* Store the message for sending events */
    self->sse_message = msg;
    self->client_connected = TRUE;
    self->event_id_counter = 0;

    /* Pause the message - we'll unpause when we have data to send */
    soup_server_message_pause (msg);

    /* Send endpoint event so the client knows where to POST messages */
    {
        g_autofree gchar *endpoint_url = NULL;

        endpoint_url = g_strdup_printf ("%s?sessionId=%s",
                                         self->post_path, self->session_id);
        send_sse_event (self, "endpoint", endpoint_url);
    }

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SESSION_ID]);
}

/*
 * Handle SSE client disconnect
 */
static void
on_sse_finished (SoupServerMessage *msg,
                 gpointer           user_data)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (user_data);

    if (self->sse_message == msg)
    {
        self->sse_message = NULL;
        self->client_connected = FALSE;

        g_free (self->session_id);
        self->session_id = NULL;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SESSION_ID]);
    }
}

/*
 * Handle POST request (client sending message)
 */
static void
handle_post_request (SoupServer        *server,
                     SoupServerMessage *msg,
                     const char        *path,
                     GHashTable        *query,
                     gpointer           user_data)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (user_data);
    SoupMessageHeaders *request_headers;
    SoupMessageHeaders *response_headers;
    const gchar *content_type;
    const gchar *session_id;
    g_autoptr(GBytes) request_body = NULL;
    const gchar *body_data;
    gsize body_len;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    /* Validate authentication */
    if (!validate_auth (self, msg))
    {
        soup_server_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED, NULL);
        return;
    }

    /* Check content type */
    request_headers = soup_server_message_get_request_headers (msg);
    content_type = soup_message_headers_get_content_type (request_headers, NULL);
    if (content_type == NULL || !g_str_has_prefix (content_type, "application/json"))
    {
        soup_server_message_set_status (msg, SOUP_STATUS_UNSUPPORTED_MEDIA_TYPE, NULL);
        return;
    }

    /* Validate session ID if we have an active SSE client.
     * Accept from Mcp-Session-Id header or sessionId query parameter. */
    if (self->client_connected && self->session_id != NULL)
    {
        session_id = soup_message_headers_get_one (request_headers, "Mcp-Session-Id");
        if (session_id == NULL && query != NULL)
        {
            session_id = (const gchar *)g_hash_table_lookup (query, "sessionId");
        }
        if (session_id == NULL || g_strcmp0 (session_id, self->session_id) != 0)
        {
            soup_server_message_set_status (msg, SOUP_STATUS_FORBIDDEN, NULL);
            return;
        }
    }

    /* Get request body */
    {
        SoupMessageBody *body = soup_server_message_get_request_body (msg);
        request_body = soup_message_body_flatten (body);
    }
    body_data = g_bytes_get_data (request_body, &body_len);

    if (body_data == NULL || body_len == 0)
    {
        soup_server_message_set_status (msg, SOUP_STATUS_BAD_REQUEST, NULL);
        return;
    }

    /* Parse JSON */
    parser = json_parser_new ();
    if (!json_parser_load_from_data (parser, body_data, body_len, &error))
    {
        g_autoptr(GError) parse_error = NULL;

        parse_error = g_error_new (MCP_ERROR, MCP_ERROR_PARSE_ERROR,
                                    "Invalid JSON: %s", error->message);
        mcp_transport_emit_error (MCP_TRANSPORT (self), parse_error);

        soup_server_message_set_status (msg, SOUP_STATUS_BAD_REQUEST, NULL);
        return;
    }

    root = json_parser_get_root (parser);
    if (root == NULL)
    {
        soup_server_message_set_status (msg, SOUP_STATUS_BAD_REQUEST, NULL);
        return;
    }

    /* Set response headers */
    response_headers = soup_server_message_get_response_headers (msg);
    soup_message_headers_replace (response_headers, "Content-Type", "application/json");

    /* Generate session ID on first request if we don't have one yet
     * (streamable HTTP mode where no SSE client connects first). */
    if (self->session_id == NULL)
    {
        self->session_id = generate_session_id ();
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SESSION_ID]);
    }
    soup_message_headers_replace (response_headers, "Mcp-Session-Id", self->session_id);

    /* Store pending POST so send_message_async can write the response
     * directly into the HTTP body (streamable HTTP / no SSE client). */
    self->pending_post_msg = msg;

    /* Emit message-received signal -- handlers run synchronously,
     * so send_message_async may clear pending_post_msg inline. */
    mcp_transport_emit_message_received (MCP_TRANSPORT (self), root);

    if (self->pending_post_msg != NULL)
    {
        /* No response was generated synchronously (notification, etc.).
         * Return 202 Accepted -- response may come via SSE later. */
        self->pending_post_msg = NULL;
        soup_server_message_set_status (msg, SOUP_STATUS_ACCEPTED, NULL);
    }
}

/*
 * Request dispatcher - routes to appropriate handler
 */
static void
handle_request (SoupServer        *server,
                SoupServerMessage *msg,
                const char        *path,
                GHashTable        *query,
                gpointer           user_data)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (user_data);
    const gchar *method;

    method = soup_server_message_get_method (msg);

    /* Route GET requests to SSE path */
    if (g_strcmp0 (method, "GET") == 0 && g_strcmp0 (path, self->sse_path) == 0)
    {
        handle_sse_request (server, msg, path, query, user_data);

        /* Connect to finished signal for cleanup */
        g_signal_connect (msg, "finished", G_CALLBACK (on_sse_finished), self);
        return;
    }

    /* Route POST requests to POST path */
    if (g_strcmp0 (method, "POST") == 0 && g_strcmp0 (path, self->post_path) == 0)
    {
        handle_post_request (server, msg, path, query, user_data);
        return;
    }

    /* Unknown path/method */
    soup_server_message_set_status (msg, SOUP_STATUS_NOT_FOUND, NULL);
}

/* McpTransport interface implementation */

static McpTransportState
mcp_http_server_transport_get_state_impl (McpTransport *transport)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (transport);
    return self->state;
}

static void
mcp_http_server_transport_connect_async (McpTransport        *transport,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;
    g_autoptr(GError) error = NULL;
    GSList *uris;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_http_server_transport_connect_async);

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

    /* Add request handler */
    soup_server_add_handler (self->server, NULL, handle_request, self, NULL);

    /* Start listening */
    if (self->host != NULL && self->host[0] != '\0')
    {
        /* Listen on specific host - use listen_local for localhost */
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
            /* For other hosts, use listen_all and filter in handler */
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
        /* Listen on all interfaces */
        if (!soup_server_listen_all (self->server, self->port, 0, &error))
        {
            g_task_return_error (task, g_steal_pointer (&error));
            set_state (self, MCP_TRANSPORT_STATE_ERROR);
            return;
        }
    }

    /* Get the actual port (for port 0 auto-assign case) */
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
mcp_http_server_transport_connect_finish (McpTransport  *transport,
                                           GAsyncResult  *result,
                                           GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_http_server_transport_disconnect_async (McpTransport        *transport,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_http_server_transport_disconnect_async);

    set_state (self, MCP_TRANSPORT_STATE_DISCONNECTING);

    stop_server (self);

    set_state (self, MCP_TRANSPORT_STATE_DISCONNECTED);

    g_task_return_boolean (task, TRUE);
}

static gboolean
mcp_http_server_transport_disconnect_finish (McpTransport  *transport,
                                              GAsyncResult  *result,
                                              GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_http_server_transport_send_message_async (McpTransport        *transport,
                                               JsonNode            *message,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;
    g_autoptr(JsonGenerator) generator = NULL;
    g_autofree gchar *json_data = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_http_server_transport_send_message_async);

    if (self->state != MCP_TRANSPORT_STATE_CONNECTED)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                  "Transport not connected");
        return;
    }

    /* Serialize JSON */
    generator = json_generator_new ();
    json_generator_set_root (generator, message);
    json_data = json_generator_to_data (generator, NULL);

    /* Streamable HTTP: if a POST is waiting for an inline response, write
     * the JSON directly into the HTTP response body instead of SSE. */
    if (self->pending_post_msg != NULL)
    {
        SoupMessageHeaders *hdrs;
        SoupMessageBody *body;

        hdrs = soup_server_message_get_response_headers (self->pending_post_msg);
        soup_message_headers_replace (hdrs, "Content-Type", "application/json");

        body = soup_server_message_get_response_body (self->pending_post_msg);
        soup_message_body_append (body, SOUP_MEMORY_COPY,
                                  json_data, strlen (json_data));

        soup_server_message_set_status (self->pending_post_msg,
                                         SOUP_STATUS_OK, NULL);
        self->pending_post_msg = NULL;

        g_task_return_boolean (task, TRUE);
        return;
    }

    if (!self->client_connected)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                  "No client connected");
        return;
    }

    /* Send as SSE event */
    if (!send_sse_event (self, "message", json_data))
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                  "Failed to send SSE event");
        return;
    }

    g_task_return_boolean (task, TRUE);
}

static gboolean
mcp_http_server_transport_send_message_finish (McpTransport  *transport,
                                                GAsyncResult  *result,
                                                GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_http_server_transport_iface_init (McpTransportInterface *iface)
{
    iface->get_state = mcp_http_server_transport_get_state_impl;
    iface->connect_async = mcp_http_server_transport_connect_async;
    iface->connect_finish = mcp_http_server_transport_connect_finish;
    iface->disconnect_async = mcp_http_server_transport_disconnect_async;
    iface->disconnect_finish = mcp_http_server_transport_disconnect_finish;
    iface->send_message_async = mcp_http_server_transport_send_message_async;
    iface->send_message_finish = mcp_http_server_transport_send_message_finish;
}

static void
stop_server (McpHttpServerTransport *self)
{
    /* Disconnect SSE client */
    if (self->sse_message != NULL)
    {
        self->sse_message = NULL;
        self->client_connected = FALSE;
    }

    /* Stop server */
    if (self->server != NULL)
    {
        soup_server_disconnect (self->server);
        g_clear_object (&self->server);
    }

    /* Clear session */
    g_clear_pointer (&self->session_id, g_free);
    self->actual_port = 0;
}

/* GObject implementation */

static void
mcp_http_server_transport_dispose (GObject *object)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (object);

    stop_server (self);
    g_clear_object (&self->tls_certificate);

    G_OBJECT_CLASS (mcp_http_server_transport_parent_class)->dispose (object);
}

static void
mcp_http_server_transport_finalize (GObject *object)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (object);

    g_free (self->host);
    g_free (self->post_path);
    g_free (self->sse_path);
    g_free (self->auth_token);
    g_free (self->session_id);

    G_OBJECT_CLASS (mcp_http_server_transport_parent_class)->finalize (object);
}

static void
mcp_http_server_transport_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (object);

    switch (prop_id)
    {
        case PROP_PORT:
            g_value_set_uint (value, self->port);
            break;
        case PROP_HOST:
            g_value_set_string (value, self->host);
            break;
        case PROP_POST_PATH:
            g_value_set_string (value, self->post_path);
            break;
        case PROP_SSE_PATH:
            g_value_set_string (value, self->sse_path);
            break;
        case PROP_SESSION_ID:
            g_value_set_string (value, self->session_id);
            break;
        case PROP_REQUIRE_AUTH:
            g_value_set_boolean (value, self->require_auth);
            break;
        case PROP_AUTH_TOKEN:
            g_value_set_string (value, self->auth_token);
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
mcp_http_server_transport_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
    McpHttpServerTransport *self = MCP_HTTP_SERVER_TRANSPORT (object);

    switch (prop_id)
    {
        case PROP_PORT:
            mcp_http_server_transport_set_port (self, g_value_get_uint (value));
            break;
        case PROP_HOST:
            mcp_http_server_transport_set_host (self, g_value_get_string (value));
            break;
        case PROP_POST_PATH:
            mcp_http_server_transport_set_post_path (self, g_value_get_string (value));
            break;
        case PROP_SSE_PATH:
            mcp_http_server_transport_set_sse_path (self, g_value_get_string (value));
            break;
        case PROP_REQUIRE_AUTH:
            mcp_http_server_transport_set_require_auth (self, g_value_get_boolean (value));
            break;
        case PROP_AUTH_TOKEN:
            mcp_http_server_transport_set_auth_token (self, g_value_get_string (value));
            break;
        case PROP_TLS_CERTIFICATE:
            mcp_http_server_transport_set_tls_certificate (self, g_value_get_object (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_http_server_transport_class_init (McpHttpServerTransportClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = mcp_http_server_transport_dispose;
    object_class->finalize = mcp_http_server_transport_finalize;
    object_class->get_property = mcp_http_server_transport_get_property;
    object_class->set_property = mcp_http_server_transport_set_property;

    /**
     * McpHttpServerTransport:port:
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
     * McpHttpServerTransport:host:
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
     * McpHttpServerTransport:post-path:
     *
     * The path for POST requests.
     */
    properties[PROP_POST_PATH] =
        g_param_spec_string ("post-path",
                             "POST Path",
                             "The path for POST requests",
                             "/",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpHttpServerTransport:sse-path:
     *
     * The path for SSE connections.
     */
    properties[PROP_SSE_PATH] =
        g_param_spec_string ("sse-path",
                             "SSE Path",
                             "The path for SSE connections",
                             "/sse",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * McpHttpServerTransport:session-id:
     *
     * The current session ID. Read-only, generated when a client connects.
     */
    properties[PROP_SESSION_ID] =
        g_param_spec_string ("session-id",
                             "Session ID",
                             "The current session ID",
                             NULL,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    /**
     * McpHttpServerTransport:require-auth:
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
     * McpHttpServerTransport:auth-token:
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
     * McpHttpServerTransport:tls-certificate:
     *
     * The TLS certificate for HTTPS.
     */
    properties[PROP_TLS_CERTIFICATE] =
        g_param_spec_object ("tls-certificate",
                             "TLS Certificate",
                             "The TLS certificate for HTTPS",
                             G_TYPE_TLS_CERTIFICATE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
mcp_http_server_transport_init (McpHttpServerTransport *self)
{
    self->state = MCP_TRANSPORT_STATE_DISCONNECTED;
    self->post_path = g_strdup ("/");
    self->sse_path = g_strdup ("/sse");
    self->require_auth = FALSE;
    self->client_connected = FALSE;
    self->event_id_counter = 0;
    self->pending_post_msg = NULL;
}

/* Public API */

/**
 * mcp_http_server_transport_new:
 * @port: the port to listen on (0 = auto-assign)
 *
 * Creates a new HTTP server transport.
 *
 * Returns: (transfer full): a new #McpHttpServerTransport
 */
McpHttpServerTransport *
mcp_http_server_transport_new (guint port)
{
    McpHttpServerTransport *self;

    self = g_object_new (MCP_TYPE_HTTP_SERVER_TRANSPORT, NULL);
    self->port = port;

    return self;
}

/**
 * mcp_http_server_transport_new_full:
 * @host: (nullable): the host/address to bind to
 * @port: the port to listen on
 *
 * Creates a new HTTP server transport with specific host binding.
 *
 * Returns: (transfer full): a new #McpHttpServerTransport
 */
McpHttpServerTransport *
mcp_http_server_transport_new_full (const gchar *host,
                                     guint        port)
{
    McpHttpServerTransport *self;

    self = g_object_new (MCP_TYPE_HTTP_SERVER_TRANSPORT, NULL);
    self->host = g_strdup (host);
    self->port = port;

    return self;
}

guint
mcp_http_server_transport_get_port (McpHttpServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self), 0);
    return self->port;
}

void
mcp_http_server_transport_set_port (McpHttpServerTransport *self,
                                     guint                   port)
{
    g_return_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self));
    g_return_if_fail (self->state == MCP_TRANSPORT_STATE_DISCONNECTED);

    self->port = port;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PORT]);
}

const gchar *
mcp_http_server_transport_get_host (McpHttpServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self), NULL);
    return self->host;
}

void
mcp_http_server_transport_set_host (McpHttpServerTransport *self,
                                     const gchar            *host)
{
    g_return_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self));
    g_return_if_fail (self->state == MCP_TRANSPORT_STATE_DISCONNECTED);

    g_free (self->host);
    self->host = g_strdup (host);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HOST]);
}

const gchar *
mcp_http_server_transport_get_post_path (McpHttpServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self), NULL);
    return self->post_path;
}

void
mcp_http_server_transport_set_post_path (McpHttpServerTransport *self,
                                          const gchar            *path)
{
    g_return_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self));

    g_free (self->post_path);
    self->post_path = g_strdup (path != NULL ? path : "/");
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POST_PATH]);
}

const gchar *
mcp_http_server_transport_get_sse_path (McpHttpServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self), NULL);
    return self->sse_path;
}

void
mcp_http_server_transport_set_sse_path (McpHttpServerTransport *self,
                                         const gchar            *path)
{
    g_return_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self));

    g_free (self->sse_path);
    self->sse_path = g_strdup (path != NULL ? path : "/sse");
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SSE_PATH]);
}

const gchar *
mcp_http_server_transport_get_session_id (McpHttpServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self), NULL);
    return self->session_id;
}

gboolean
mcp_http_server_transport_get_require_auth (McpHttpServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self), FALSE);
    return self->require_auth;
}

void
mcp_http_server_transport_set_require_auth (McpHttpServerTransport *self,
                                             gboolean                require_auth)
{
    g_return_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self));

    self->require_auth = require_auth;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REQUIRE_AUTH]);
}

const gchar *
mcp_http_server_transport_get_auth_token (McpHttpServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self), NULL);
    return self->auth_token;
}

void
mcp_http_server_transport_set_auth_token (McpHttpServerTransport *self,
                                           const gchar            *token)
{
    g_return_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self));

    g_free (self->auth_token);
    self->auth_token = g_strdup (token);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTH_TOKEN]);
}

GTlsCertificate *
mcp_http_server_transport_get_tls_certificate (McpHttpServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self), NULL);
    return self->tls_certificate;
}

void
mcp_http_server_transport_set_tls_certificate (McpHttpServerTransport *self,
                                                GTlsCertificate        *certificate)
{
    g_return_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self));
    g_return_if_fail (self->state == MCP_TRANSPORT_STATE_DISCONNECTED);

    g_set_object (&self->tls_certificate, certificate);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TLS_CERTIFICATE]);
}

SoupServer *
mcp_http_server_transport_get_soup_server (McpHttpServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self), NULL);
    return self->server;
}

gboolean
mcp_http_server_transport_has_client (McpHttpServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self), FALSE);
    return self->client_connected;
}

guint
mcp_http_server_transport_get_actual_port (McpHttpServerTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_SERVER_TRANSPORT (self), 0);
    return self->actual_port;
}
