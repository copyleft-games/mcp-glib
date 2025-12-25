/*
 * mcp-http-transport.c - HTTP transport implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define MCP_COMPILATION
#include <mcp/mcp-http-transport.h>
#include <mcp/mcp-error.h>
#undef MCP_COMPILATION

#include <string.h>

/**
 * SECTION:mcp-http-transport
 * @title: McpHttpTransport
 * @short_description: HTTP transport implementation
 *
 * #McpHttpTransport provides an HTTP-based transport for MCP communication.
 * It uses HTTP POST requests to send messages and Server-Sent Events (SSE)
 * for receiving messages from the server.
 */

struct _McpHttpTransport
{
    GObject parent_instance;

    /* Configuration */
    gchar *base_url;
    gchar *auth_token;
    gchar *sse_endpoint;
    gchar *post_endpoint;
    guint  timeout_seconds;
    gboolean reconnect_enabled;
    guint    reconnect_delay_ms;

    /* Session state */
    gchar *session_id;
    gchar *last_event_id;

    /* Soup session */
    SoupSession *session;
    gboolean owns_session;

    /* SSE connection */
    SoupMessage *sse_message;
    GInputStream *sse_stream;
    GDataInputStream *sse_data_stream;
    GCancellable *sse_cancellable;
    gboolean sse_reading;

    /* State */
    McpTransportState state;

    /* Pending reconnect */
    guint reconnect_timeout_id;
};

static void mcp_http_transport_iface_init (McpTransportInterface *iface);

G_DEFINE_TYPE_WITH_CODE (McpHttpTransport, mcp_http_transport, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MCP_TYPE_TRANSPORT,
                                                mcp_http_transport_iface_init))

enum
{
    PROP_0,
    PROP_BASE_URL,
    PROP_AUTH_TOKEN,
    PROP_SESSION_ID,
    PROP_TIMEOUT,
    PROP_SSE_ENDPOINT,
    PROP_POST_ENDPOINT,
    PROP_RECONNECT_ENABLED,
    PROP_RECONNECT_DELAY,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

/* Forward declarations */
static void start_sse_connection (McpHttpTransport *self);
static void stop_sse_connection (McpHttpTransport *self);
static void schedule_reconnect (McpHttpTransport *self);

static void
set_state (McpHttpTransport  *self,
           McpTransportState  new_state)
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

static gchar *
build_url (McpHttpTransport *self,
           const gchar      *endpoint)
{
    if (endpoint == NULL || endpoint[0] == '\0')
    {
        return g_strdup (self->base_url);
    }

    if (endpoint[0] == '/')
    {
        /* Absolute path - append to base URL origin */
        g_autoptr(GUri) base_uri = NULL;
        g_autoptr(GUri) new_uri = NULL;

        base_uri = g_uri_parse (self->base_url, G_URI_FLAGS_NONE, NULL);
        if (base_uri == NULL)
        {
            return g_strconcat (self->base_url, endpoint, NULL);
        }

        new_uri = g_uri_build (g_uri_get_flags (base_uri),
                               g_uri_get_scheme (base_uri),
                               g_uri_get_userinfo (base_uri),
                               g_uri_get_host (base_uri),
                               g_uri_get_port (base_uri),
                               endpoint,
                               g_uri_get_query (base_uri),
                               g_uri_get_fragment (base_uri));

        return g_uri_to_string (new_uri);
    }

    /* Relative path - append to base URL */
    if (g_str_has_suffix (self->base_url, "/"))
    {
        return g_strconcat (self->base_url, endpoint, NULL);
    }
    else
    {
        return g_strconcat (self->base_url, "/", endpoint, NULL);
    }
}

static void
add_auth_header (McpHttpTransport *self,
                 SoupMessage      *message)
{
    SoupMessageHeaders *headers;

    headers = soup_message_get_request_headers (message);

    if (self->auth_token != NULL && self->auth_token[0] != '\0')
    {
        g_autofree gchar *auth_value = NULL;
        auth_value = g_strdup_printf ("Bearer %s", self->auth_token);
        soup_message_headers_replace (headers, "Authorization", auth_value);
    }

    if (self->session_id != NULL && self->session_id[0] != '\0')
    {
        soup_message_headers_replace (headers, "Mcp-Session-Id", self->session_id);
    }
}

/* SSE parsing */

static void
process_sse_event (McpHttpTransport *self,
                   const gchar      *event_type,
                   const gchar      *data,
                   const gchar      *event_id)
{
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    /* Update last event ID for reconnection */
    if (event_id != NULL && event_id[0] != '\0')
    {
        g_free (self->last_event_id);
        self->last_event_id = g_strdup (event_id);
    }

    /* Skip if no data */
    if (data == NULL || data[0] == '\0')
    {
        return;
    }

    /* Parse JSON data */
    parser = json_parser_new ();
    if (!json_parser_load_from_data (parser, data, -1, &error))
    {
        g_warning ("Failed to parse SSE data as JSON: %s", error->message);
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
read_sse_line_cb (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data);

static void
continue_sse_reading (McpHttpTransport *self)
{
    if (self->sse_data_stream == NULL || !self->sse_reading)
    {
        return;
    }

    g_data_input_stream_read_line_async (self->sse_data_stream,
                                          G_PRIORITY_DEFAULT,
                                          self->sse_cancellable,
                                          read_sse_line_cb,
                                          self);
}

static void
read_sse_line_cb (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    McpHttpTransport *self = MCP_HTTP_TRANSPORT (user_data);
    g_autoptr(GError) error = NULL;
    g_autofree gchar *line = NULL;
    gsize length;

    /* Static buffers for building events */
    static GString *event_type = NULL;
    static GString *event_data = NULL;
    static GString *event_id = NULL;

    if (event_type == NULL)
    {
        event_type = g_string_new (NULL);
        event_data = g_string_new (NULL);
        event_id = g_string_new (NULL);
    }

    line = g_data_input_stream_read_line_finish (G_DATA_INPUT_STREAM (source),
                                                  result, &length, &error);

    if (line == NULL)
    {
        if (error != NULL)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            {
                g_warning ("SSE read error: %s", error->message);
                mcp_transport_emit_error (MCP_TRANSPORT (self), error);

                if (self->reconnect_enabled && self->state == MCP_TRANSPORT_STATE_CONNECTED)
                {
                    schedule_reconnect (self);
                }
                else
                {
                    set_state (self, MCP_TRANSPORT_STATE_ERROR);
                }
            }
        }
        else
        {
            /* EOF - connection closed */
            if (self->reconnect_enabled && self->state == MCP_TRANSPORT_STATE_CONNECTED)
            {
                schedule_reconnect (self);
            }
            else
            {
                set_state (self, MCP_TRANSPORT_STATE_DISCONNECTED);
            }
        }

        self->sse_reading = FALSE;
        return;
    }

    /* Parse SSE line */
    if (length == 0)
    {
        /* Empty line = dispatch event */
        if (event_data->len > 0)
        {
            /* Remove trailing newline from data if present */
            if (event_data->len > 0 && event_data->str[event_data->len - 1] == '\n')
            {
                g_string_truncate (event_data, event_data->len - 1);
            }

            process_sse_event (self,
                               event_type->len > 0 ? event_type->str : "message",
                               event_data->str,
                               event_id->len > 0 ? event_id->str : NULL);
        }

        /* Reset for next event */
        g_string_truncate (event_type, 0);
        g_string_truncate (event_data, 0);
        g_string_truncate (event_id, 0);
    }
    else if (line[0] == ':')
    {
        /* Comment - ignore */
    }
    else if (g_str_has_prefix (line, "event:"))
    {
        const gchar *value = line + 6;
        while (*value == ' ') value++;
        g_string_assign (event_type, value);
    }
    else if (g_str_has_prefix (line, "data:"))
    {
        const gchar *value = line + 5;
        while (*value == ' ') value++;
        if (event_data->len > 0)
        {
            g_string_append_c (event_data, '\n');
        }
        g_string_append (event_data, value);
    }
    else if (g_str_has_prefix (line, "id:"))
    {
        const gchar *value = line + 3;
        while (*value == ' ') value++;
        g_string_assign (event_id, value);
    }
    else if (g_str_has_prefix (line, "retry:"))
    {
        const gchar *value = line + 6;
        while (*value == ' ') value++;
        self->reconnect_delay_ms = (guint) g_ascii_strtoull (value, NULL, 10);
    }

    /* Continue reading */
    continue_sse_reading (self);
}

static void
sse_send_cb (GObject      *source,
             GAsyncResult *result,
             gpointer      user_data)
{
    McpHttpTransport *self = MCP_HTTP_TRANSPORT (user_data);
    g_autoptr(GError) error = NULL;
    g_autoptr(GInputStream) stream = NULL;
    SoupMessageHeaders *response_headers;
    const gchar *session_id;
    guint status;

    stream = soup_session_send_finish (SOUP_SESSION (source), result, &error);

    if (stream == NULL)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            g_warning ("SSE connection failed: %s", error->message);
            mcp_transport_emit_error (MCP_TRANSPORT (self), error);

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

    status = soup_message_get_status (self->sse_message);
    if (status != SOUP_STATUS_OK)
    {
        g_autoptr(GError) http_error = NULL;

        http_error = g_error_new (MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                   "SSE connection failed with status %u: %s",
                                   status, soup_message_get_reason_phrase (self->sse_message));
        mcp_transport_emit_error (MCP_TRANSPORT (self), http_error);

        if (self->reconnect_enabled)
        {
            schedule_reconnect (self);
        }
        else
        {
            set_state (self, MCP_TRANSPORT_STATE_ERROR);
        }
        return;
    }

    /* Extract session ID from response headers */
    response_headers = soup_message_get_response_headers (self->sse_message);
    session_id = soup_message_headers_get_one (response_headers, "Mcp-Session-Id");
    if (session_id != NULL)
    {
        g_free (self->session_id);
        self->session_id = g_strdup (session_id);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SESSION_ID]);
    }

    /* Store stream for reading */
    self->sse_stream = g_object_ref (stream);
    self->sse_data_stream = g_data_input_stream_new (stream);
    self->sse_reading = TRUE;

    set_state (self, MCP_TRANSPORT_STATE_CONNECTED);

    /* Start reading SSE events */
    continue_sse_reading (self);
}

static void
start_sse_connection (McpHttpTransport *self)
{
    g_autofree gchar *url = NULL;
    SoupMessageHeaders *headers;

    url = build_url (self, self->sse_endpoint);

    self->sse_message = soup_message_new ("GET", url);
    if (self->sse_message == NULL)
    {
        g_autoptr(GError) error = NULL;

        error = g_error_new (MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                              "Invalid SSE URL: %s", url);
        mcp_transport_emit_error (MCP_TRANSPORT (self), error);
        set_state (self, MCP_TRANSPORT_STATE_ERROR);
        return;
    }

    headers = soup_message_get_request_headers (self->sse_message);
    soup_message_headers_replace (headers, "Accept", "text/event-stream");
    soup_message_headers_replace (headers, "Cache-Control", "no-cache");

    /* Add Last-Event-ID for reconnection */
    if (self->last_event_id != NULL && self->last_event_id[0] != '\0')
    {
        soup_message_headers_replace (headers, "Last-Event-ID", self->last_event_id);
    }

    add_auth_header (self, self->sse_message);

    self->sse_cancellable = g_cancellable_new ();

    soup_session_send_async (self->session,
                              self->sse_message,
                              G_PRIORITY_DEFAULT,
                              self->sse_cancellable,
                              sse_send_cb,
                              self);
}

static void
stop_sse_connection (McpHttpTransport *self)
{
    self->sse_reading = FALSE;

    if (self->sse_cancellable != NULL)
    {
        g_cancellable_cancel (self->sse_cancellable);
        g_clear_object (&self->sse_cancellable);
    }

    g_clear_object (&self->sse_data_stream);
    g_clear_object (&self->sse_stream);
    g_clear_object (&self->sse_message);

    if (self->reconnect_timeout_id > 0)
    {
        g_source_remove (self->reconnect_timeout_id);
        self->reconnect_timeout_id = 0;
    }
}

static gboolean
reconnect_timeout_cb (gpointer user_data)
{
    McpHttpTransport *self = MCP_HTTP_TRANSPORT (user_data);

    self->reconnect_timeout_id = 0;

    if (self->state != MCP_TRANSPORT_STATE_DISCONNECTED)
    {
        start_sse_connection (self);
    }

    return G_SOURCE_REMOVE;
}

static void
schedule_reconnect (McpHttpTransport *self)
{
    stop_sse_connection (self);
    set_state (self, MCP_TRANSPORT_STATE_CONNECTING);

    self->reconnect_timeout_id = g_timeout_add (self->reconnect_delay_ms,
                                                 reconnect_timeout_cb,
                                                 self);
}

/* McpTransport interface implementation */

static McpTransportState
mcp_http_transport_get_state_impl (McpTransport *transport)
{
    McpHttpTransport *self = MCP_HTTP_TRANSPORT (transport);
    return self->state;
}

static void
mcp_http_transport_connect_async (McpTransport        *transport,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
    McpHttpTransport *self = MCP_HTTP_TRANSPORT (transport);
    GTask *task;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_http_transport_connect_async);

    if (self->state != MCP_TRANSPORT_STATE_DISCONNECTED)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                  "Transport already connecting or connected");
        g_object_unref (task);
        return;
    }

    if (self->base_url == NULL || self->base_url[0] == '\0')
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                  "No base URL set");
        g_object_unref (task);
        return;
    }

    set_state (self, MCP_TRANSPORT_STATE_CONNECTING);

    /* Start SSE connection */
    start_sse_connection (self);

    /* Return success immediately - the SSE connection will update state */
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static gboolean
mcp_http_transport_connect_finish (McpTransport  *transport,
                                    GAsyncResult  *result,
                                    GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_http_transport_disconnect_async (McpTransport        *transport,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
    McpHttpTransport *self = MCP_HTTP_TRANSPORT (transport);
    GTask *task;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_http_transport_disconnect_async);

    set_state (self, MCP_TRANSPORT_STATE_DISCONNECTING);

    stop_sse_connection (self);

    /* Clear session state */
    g_clear_pointer (&self->session_id, g_free);
    g_clear_pointer (&self->last_event_id, g_free);

    set_state (self, MCP_TRANSPORT_STATE_DISCONNECTED);

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static gboolean
mcp_http_transport_disconnect_finish (McpTransport  *transport,
                                       GAsyncResult  *result,
                                       GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct
{
    McpHttpTransport *transport;
    GTask *task;
    JsonNode *message;
} SendMessageData;

static void
send_message_data_free (SendMessageData *data)
{
    g_object_unref (data->transport);
    json_node_unref (data->message);
    g_slice_free (SendMessageData, data);
}

static void
post_send_cb (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
    SendMessageData *data = user_data;
    g_autoptr(GError) error = NULL;
    g_autoptr(GBytes) response = NULL;
    guint status;
    SoupMessage *message;

    response = soup_session_send_and_read_finish (SOUP_SESSION (source), result, &error);

    message = g_object_get_data (source, "current-message");

    if (response == NULL)
    {
        g_task_return_error (data->task, g_steal_pointer (&error));
        g_object_unref (data->task);
        send_message_data_free (data);
        return;
    }

    status = soup_message_get_status (message);

    if (status >= 400)
    {
        g_task_return_new_error (data->task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                  "HTTP POST failed with status %u: %s",
                                  status, soup_message_get_reason_phrase (message));
    }
    else
    {
        /* Check if response has JSON that needs to be processed */
        SoupMessageHeaders *headers;
        const gchar *content_type;

        headers = soup_message_get_response_headers (message);
        content_type = soup_message_headers_get_content_type (headers, NULL);

        if (content_type != NULL && g_str_has_prefix (content_type, "application/json"))
        {
            /* Parse and emit the response */
            g_autoptr(JsonParser) parser = NULL;
            const gchar *response_data;
            gsize response_len;

            response_data = g_bytes_get_data (response, &response_len);

            if (response_len > 0)
            {
                parser = json_parser_new ();
                if (json_parser_load_from_data (parser, response_data, response_len, NULL))
                {
                    JsonNode *root = json_parser_get_root (parser);
                    if (root != NULL)
                    {
                        mcp_transport_emit_message_received (MCP_TRANSPORT (data->transport), root);
                    }
                }
            }
        }

        g_task_return_boolean (data->task, TRUE);
    }

    g_object_unref (data->task);
    send_message_data_free (data);
}

static void
mcp_http_transport_send_message_async (McpTransport        *transport,
                                        JsonNode            *message,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
    McpHttpTransport *self = MCP_HTTP_TRANSPORT (transport);
    GTask *task;
    SendMessageData *data;
    g_autofree gchar *url = NULL;
    g_autofree gchar *json_data = NULL;
    g_autoptr(JsonGenerator) generator = NULL;
    SoupMessage *soup_msg;
    SoupMessageHeaders *headers;
    GBytes *body;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_http_transport_send_message_async);

    if (self->state != MCP_TRANSPORT_STATE_CONNECTED &&
        self->state != MCP_TRANSPORT_STATE_CONNECTING)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                  "Transport not connected");
        g_object_unref (task);
        return;
    }

    /* Serialize JSON */
    generator = json_generator_new ();
    json_generator_set_root (generator, message);
    json_data = json_generator_to_data (generator, NULL);

    /* Build URL */
    url = build_url (self, self->post_endpoint);

    /* Create POST request */
    soup_msg = soup_message_new ("POST", url);
    if (soup_msg == NULL)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                  "Invalid POST URL: %s", url);
        g_object_unref (task);
        return;
    }

    headers = soup_message_get_request_headers (soup_msg);
    soup_message_headers_replace (headers, "Content-Type", "application/json");
    soup_message_headers_replace (headers, "Accept", "application/json, text/event-stream");

    add_auth_header (self, soup_msg);

    body = g_bytes_new (json_data, strlen (json_data));
    soup_message_set_request_body_from_bytes (soup_msg, "application/json", body);
    g_bytes_unref (body);

    /* Store message for callback */
    g_object_set_data_full (G_OBJECT (self->session), "current-message",
                            soup_msg, g_object_unref);

    data = g_slice_new (SendMessageData);
    data->transport = g_object_ref (self);
    data->task = task;
    data->message = json_node_copy (message);

    soup_session_send_and_read_async (self->session,
                                       soup_msg,
                                       G_PRIORITY_DEFAULT,
                                       cancellable,
                                       post_send_cb,
                                       data);
}

static gboolean
mcp_http_transport_send_message_finish (McpTransport  *transport,
                                         GAsyncResult  *result,
                                         GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_http_transport_iface_init (McpTransportInterface *iface)
{
    iface->get_state = mcp_http_transport_get_state_impl;
    iface->connect_async = mcp_http_transport_connect_async;
    iface->connect_finish = mcp_http_transport_connect_finish;
    iface->disconnect_async = mcp_http_transport_disconnect_async;
    iface->disconnect_finish = mcp_http_transport_disconnect_finish;
    iface->send_message_async = mcp_http_transport_send_message_async;
    iface->send_message_finish = mcp_http_transport_send_message_finish;
}

/* GObject implementation */

static void
mcp_http_transport_dispose (GObject *object)
{
    McpHttpTransport *self = MCP_HTTP_TRANSPORT (object);

    stop_sse_connection (self);

    if (self->owns_session)
    {
        g_clear_object (&self->session);
    }
    else
    {
        self->session = NULL;
    }

    G_OBJECT_CLASS (mcp_http_transport_parent_class)->dispose (object);
}

static void
mcp_http_transport_finalize (GObject *object)
{
    McpHttpTransport *self = MCP_HTTP_TRANSPORT (object);

    g_free (self->base_url);
    g_free (self->auth_token);
    g_free (self->sse_endpoint);
    g_free (self->post_endpoint);
    g_free (self->session_id);
    g_free (self->last_event_id);

    G_OBJECT_CLASS (mcp_http_transport_parent_class)->finalize (object);
}

static void
mcp_http_transport_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
    McpHttpTransport *self = MCP_HTTP_TRANSPORT (object);

    switch (prop_id)
    {
        case PROP_BASE_URL:
            g_value_set_string (value, self->base_url);
            break;
        case PROP_AUTH_TOKEN:
            g_value_set_string (value, self->auth_token);
            break;
        case PROP_SESSION_ID:
            g_value_set_string (value, self->session_id);
            break;
        case PROP_TIMEOUT:
            g_value_set_uint (value, self->timeout_seconds);
            break;
        case PROP_SSE_ENDPOINT:
            g_value_set_string (value, self->sse_endpoint);
            break;
        case PROP_POST_ENDPOINT:
            g_value_set_string (value, self->post_endpoint);
            break;
        case PROP_RECONNECT_ENABLED:
            g_value_set_boolean (value, self->reconnect_enabled);
            break;
        case PROP_RECONNECT_DELAY:
            g_value_set_uint (value, self->reconnect_delay_ms);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_http_transport_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
    McpHttpTransport *self = MCP_HTTP_TRANSPORT (object);

    switch (prop_id)
    {
        case PROP_BASE_URL:
            mcp_http_transport_set_base_url (self, g_value_get_string (value));
            break;
        case PROP_AUTH_TOKEN:
            mcp_http_transport_set_auth_token (self, g_value_get_string (value));
            break;
        case PROP_TIMEOUT:
            mcp_http_transport_set_timeout (self, g_value_get_uint (value));
            break;
        case PROP_SSE_ENDPOINT:
            mcp_http_transport_set_sse_endpoint (self, g_value_get_string (value));
            break;
        case PROP_POST_ENDPOINT:
            mcp_http_transport_set_post_endpoint (self, g_value_get_string (value));
            break;
        case PROP_RECONNECT_ENABLED:
            mcp_http_transport_set_reconnect_enabled (self, g_value_get_boolean (value));
            break;
        case PROP_RECONNECT_DELAY:
            mcp_http_transport_set_reconnect_delay (self, g_value_get_uint (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_http_transport_class_init (McpHttpTransportClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = mcp_http_transport_dispose;
    object_class->finalize = mcp_http_transport_finalize;
    object_class->get_property = mcp_http_transport_get_property;
    object_class->set_property = mcp_http_transport_set_property;

    properties[PROP_BASE_URL] =
        g_param_spec_string ("base-url",
                             "Base URL",
                             "The base URL for the MCP server",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_AUTH_TOKEN] =
        g_param_spec_string ("auth-token",
                             "Auth Token",
                             "The authentication token",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SESSION_ID] =
        g_param_spec_string ("session-id",
                             "Session ID",
                             "The MCP session ID from the server",
                             NULL,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_TIMEOUT] =
        g_param_spec_uint ("timeout",
                           "Timeout",
                           "Request timeout in seconds",
                           0, G_MAXUINT, 30,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SSE_ENDPOINT] =
        g_param_spec_string ("sse-endpoint",
                             "SSE Endpoint",
                             "The SSE endpoint path",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_POST_ENDPOINT] =
        g_param_spec_string ("post-endpoint",
                             "POST Endpoint",
                             "The POST endpoint path",
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
                           "Reconnect delay in milliseconds",
                           0, G_MAXUINT, 3000,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
mcp_http_transport_init (McpHttpTransport *self)
{
    self->state = MCP_TRANSPORT_STATE_DISCONNECTED;
    self->timeout_seconds = 30;
    self->reconnect_enabled = TRUE;
    self->reconnect_delay_ms = 3000;
}

/* Public API */

McpHttpTransport *
mcp_http_transport_new (const gchar *base_url)
{
    McpHttpTransport *self;

    g_return_val_if_fail (base_url != NULL, NULL);

    self = g_object_new (MCP_TYPE_HTTP_TRANSPORT, NULL);
    self->base_url = g_strdup (base_url);
    self->session = soup_session_new ();
    self->owns_session = TRUE;

    return self;
}

McpHttpTransport *
mcp_http_transport_new_with_session (const gchar *base_url,
                                      SoupSession *session)
{
    McpHttpTransport *self;

    g_return_val_if_fail (base_url != NULL, NULL);
    g_return_val_if_fail (SOUP_IS_SESSION (session), NULL);

    self = g_object_new (MCP_TYPE_HTTP_TRANSPORT, NULL);
    self->base_url = g_strdup (base_url);
    self->session = session;
    self->owns_session = FALSE;

    return self;
}

const gchar *
mcp_http_transport_get_base_url (McpHttpTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_TRANSPORT (self), NULL);
    return self->base_url;
}

void
mcp_http_transport_set_base_url (McpHttpTransport *self,
                                  const gchar      *base_url)
{
    g_return_if_fail (MCP_IS_HTTP_TRANSPORT (self));
    g_return_if_fail (self->state == MCP_TRANSPORT_STATE_DISCONNECTED);

    g_free (self->base_url);
    self->base_url = g_strdup (base_url);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BASE_URL]);
}

const gchar *
mcp_http_transport_get_auth_token (McpHttpTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_TRANSPORT (self), NULL);
    return self->auth_token;
}

void
mcp_http_transport_set_auth_token (McpHttpTransport *self,
                                    const gchar      *token)
{
    g_return_if_fail (MCP_IS_HTTP_TRANSPORT (self));

    g_free (self->auth_token);
    self->auth_token = g_strdup (token);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTH_TOKEN]);
}

const gchar *
mcp_http_transport_get_session_id (McpHttpTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_TRANSPORT (self), NULL);
    return self->session_id;
}

SoupSession *
mcp_http_transport_get_soup_session (McpHttpTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_TRANSPORT (self), NULL);
    return self->session;
}

void
mcp_http_transport_set_timeout (McpHttpTransport *self,
                                 guint             timeout_seconds)
{
    g_return_if_fail (MCP_IS_HTTP_TRANSPORT (self));

    self->timeout_seconds = timeout_seconds;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIMEOUT]);
}

guint
mcp_http_transport_get_timeout (McpHttpTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_TRANSPORT (self), 0);
    return self->timeout_seconds;
}

void
mcp_http_transport_set_sse_endpoint (McpHttpTransport *self,
                                      const gchar      *endpoint)
{
    g_return_if_fail (MCP_IS_HTTP_TRANSPORT (self));

    g_free (self->sse_endpoint);
    self->sse_endpoint = g_strdup (endpoint);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SSE_ENDPOINT]);
}

const gchar *
mcp_http_transport_get_sse_endpoint (McpHttpTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_TRANSPORT (self), NULL);
    return self->sse_endpoint;
}

void
mcp_http_transport_set_post_endpoint (McpHttpTransport *self,
                                       const gchar      *endpoint)
{
    g_return_if_fail (MCP_IS_HTTP_TRANSPORT (self));

    g_free (self->post_endpoint);
    self->post_endpoint = g_strdup (endpoint);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POST_ENDPOINT]);
}

const gchar *
mcp_http_transport_get_post_endpoint (McpHttpTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_TRANSPORT (self), NULL);
    return self->post_endpoint;
}

void
mcp_http_transport_set_reconnect_enabled (McpHttpTransport *self,
                                           gboolean          enabled)
{
    g_return_if_fail (MCP_IS_HTTP_TRANSPORT (self));

    self->reconnect_enabled = enabled;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RECONNECT_ENABLED]);
}

gboolean
mcp_http_transport_get_reconnect_enabled (McpHttpTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_TRANSPORT (self), FALSE);
    return self->reconnect_enabled;
}

void
mcp_http_transport_set_reconnect_delay (McpHttpTransport *self,
                                         guint             delay_ms)
{
    g_return_if_fail (MCP_IS_HTTP_TRANSPORT (self));

    self->reconnect_delay_ms = delay_ms;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RECONNECT_DELAY]);
}

guint
mcp_http_transport_get_reconnect_delay (McpHttpTransport *self)
{
    g_return_val_if_fail (MCP_IS_HTTP_TRANSPORT (self), 0);
    return self->reconnect_delay_ms;
}
