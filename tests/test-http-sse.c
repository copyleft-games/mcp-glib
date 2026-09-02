/*
 * test-http-sse.c - End-to-end tests for the HTTP transports' SSE stream
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The HTTP transports are covered elsewhere only by property and
 * construction tests, so nothing had ever driven a byte between them.
 * Everything here is about the stream after the first event: a response
 * that libsoup closes because nothing asked for chunked encoding, event
 * state kept in function-level statics that two clients share, and a
 * server whose SSE lines end in CRLF.
 */

#define MCP_COMPILATION
#include "mcp.h"
#include "mcp-http-transport.h"
#include "mcp-http-server-transport.h"
#undef MCP_COMPILATION

#include <libsoup/soup.h>
#include <string.h>

/* ========================================================================== */
/* Helpers                                                                    */
/* ========================================================================== */

typedef struct
{
    GPtrArray *received;    /* gchar * — serialised inbound frames */
    GError    *last_error;
} Peer;

static Peer *
peer_new (void)
{
    Peer *peer = g_new0 (Peer, 1);

    peer->received = g_ptr_array_new_with_free_func (g_free);
    return peer;
}

static void
peer_free (Peer *peer)
{
    g_ptr_array_unref (peer->received);
    g_clear_error (&peer->last_error);
    g_free (peer);
}

static void
on_peer_message (McpTransport *transport,
                 JsonNode     *message,
                 gpointer      user_data)
{
    Peer *peer = user_data;
    g_autoptr(JsonGenerator) generator = json_generator_new ();

    (void) transport;

    json_generator_set_root (generator, message);
    g_ptr_array_add (peer->received, json_generator_to_data (generator, NULL));
}

static void
on_peer_error (McpTransport *transport,
               GError       *error,
               gpointer      user_data)
{
    Peer *peer = user_data;

    (void) transport;

    g_clear_error (&peer->last_error);
    peer->last_error = g_error_copy (error);
}

static void
pump (guint milliseconds)
{
    gint64 deadline = g_get_monotonic_time () + milliseconds * 1000;

    while (g_get_monotonic_time () < deadline)
    {
        if (!g_main_context_iteration (NULL, FALSE))
        {
            g_usleep (500);
        }
    }
}

static gboolean
pump_until_frames (Peer  *peer,
                   guint  count,
                   guint  milliseconds)
{
    gint64 deadline = g_get_monotonic_time () + milliseconds * 1000;

    while (peer->received->len < count && g_get_monotonic_time () < deadline)
    {
        if (!g_main_context_iteration (NULL, FALSE))
        {
            g_usleep (500);
        }
    }

    return peer->received->len >= count;
}

static JsonNode *
make_notification (const gchar *method)
{
    g_autoptr(JsonBuilder) builder = json_builder_new ();

    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "jsonrpc");
    json_builder_add_string_value (builder, "2.0");
    json_builder_set_member_name (builder, "method");
    json_builder_add_string_value (builder, method);
    json_builder_end_object (builder);

    return json_builder_get_root (builder);
}

static gboolean
peer_saw (Peer        *peer,
          const gchar *needle)
{
    guint i;

    for (i = 0; i < peer->received->len; i++)
    {
        if (strstr (g_ptr_array_index (peer->received, i), needle) != NULL)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/* Connect a transport and turn the loop until it says so. */
static gboolean
connect_transport (McpTransport *transport,
                   guint         milliseconds)
{
    gint64 deadline;

    mcp_transport_connect_async (transport, NULL, NULL, NULL);

    deadline = g_get_monotonic_time () + milliseconds * 1000;
    while (!mcp_transport_is_connected (transport)
           && g_get_monotonic_time () < deadline)
    {
        if (!g_main_context_iteration (NULL, FALSE))
        {
            g_usleep (500);
        }
    }

    return mcp_transport_is_connected (transport);
}

/* ========================================================================== */
/* A server-initiated stream is more than one event                           */
/* ========================================================================== */

/*
 * The SSE handler set the status, appended the endpoint event and
 * unpaused -- but never asked libsoup for chunked encoding, so libsoup
 * took the body length at handler return as the Content-Length and
 * finished the response.  The client saw the endpoint event and then EOF,
 * and every later server-initiated message came back "No client
 * connected".
 */
static void
test_server_sends_more_than_one_event (void)
{
    g_autoptr(McpHttpServerTransport) server = NULL;
    g_autoptr(McpHttpTransport) client = NULL;
    g_autofree gchar *url = NULL;
    Peer *peer = peer_new ();
    g_autoptr(JsonNode) first = make_notification ("first");
    g_autoptr(JsonNode) second = make_notification ("second");

    server = mcp_http_server_transport_new_full ("127.0.0.1", 0);
    g_assert_true (connect_transport (MCP_TRANSPORT (server), 2000));

    url = g_strdup_printf ("http://127.0.0.1:%u",
                           mcp_http_server_transport_get_actual_port (server));
    client = mcp_http_transport_new (url);
    mcp_http_transport_set_sse_endpoint (client, "/sse");
    g_signal_connect (client, "message-received", G_CALLBACK (on_peer_message), peer);
    g_signal_connect (client, "error", G_CALLBACK (on_peer_error), peer);

    g_assert_true (connect_transport (MCP_TRANSPORT (client), 2000));

    /* Let the endpoint event land so the server knows it has a client. */
    pump (200);
    g_assert_true (mcp_http_server_transport_has_client (server));

    mcp_transport_send_message_async (MCP_TRANSPORT (server), first, NULL, NULL, NULL);
    mcp_transport_send_message_async (MCP_TRANSPORT (server), second, NULL, NULL, NULL);

    g_assert_true (pump_until_frames (peer, 2, 3000));
    g_assert_true (peer_saw (peer, "\"first\""));
    g_assert_true (peer_saw (peer, "\"second\""));

    mcp_transport_disconnect_async (MCP_TRANSPORT (client), NULL, NULL, NULL);
    mcp_transport_disconnect_async (MCP_TRANSPORT (server), NULL, NULL, NULL);
    pump (100);

    peer_free (peer);
}

/* ========================================================================== */
/* Two clients must not share one event buffer                                */
/* ========================================================================== */

/*
 * read_sse_line_cb() accumulated the event type, data and id in three
 * function-level `static GString *`, which is process-global.  Two
 * clients against two servers interleave their reads on one main loop and
 * hand each other's payloads over.
 */
static void
test_two_clients_do_not_share_event_state (void)
{
    g_autoptr(McpHttpServerTransport) server_a = NULL;
    g_autoptr(McpHttpServerTransport) server_b = NULL;
    g_autoptr(McpHttpTransport) client_a = NULL;
    g_autoptr(McpHttpTransport) client_b = NULL;
    g_autofree gchar *url_a = NULL;
    g_autofree gchar *url_b = NULL;
    Peer *peer_a = peer_new ();
    Peer *peer_b = peer_new ();
    g_autoptr(JsonNode) for_a = make_notification ("only-for-a");
    g_autoptr(JsonNode) for_b = make_notification ("only-for-b");

    server_a = mcp_http_server_transport_new_full ("127.0.0.1", 0);
    server_b = mcp_http_server_transport_new_full ("127.0.0.1", 0);
    g_assert_true (connect_transport (MCP_TRANSPORT (server_a), 2000));
    g_assert_true (connect_transport (MCP_TRANSPORT (server_b), 2000));

    url_a = g_strdup_printf ("http://127.0.0.1:%u",
                             mcp_http_server_transport_get_actual_port (server_a));
    url_b = g_strdup_printf ("http://127.0.0.1:%u",
                             mcp_http_server_transport_get_actual_port (server_b));

    client_a = mcp_http_transport_new (url_a);
    client_b = mcp_http_transport_new (url_b);
    mcp_http_transport_set_sse_endpoint (client_a, "/sse");
    mcp_http_transport_set_sse_endpoint (client_b, "/sse");
    g_signal_connect (client_a, "message-received", G_CALLBACK (on_peer_message), peer_a);
    g_signal_connect (client_b, "message-received", G_CALLBACK (on_peer_message), peer_b);

    g_assert_true (connect_transport (MCP_TRANSPORT (client_a), 2000));
    g_assert_true (connect_transport (MCP_TRANSPORT (client_b), 2000));
    pump (200);

    mcp_transport_send_message_async (MCP_TRANSPORT (server_a), for_a, NULL, NULL, NULL);
    mcp_transport_send_message_async (MCP_TRANSPORT (server_b), for_b, NULL, NULL, NULL);

    g_assert_true (pump_until_frames (peer_a, 1, 3000));
    g_assert_true (pump_until_frames (peer_b, 1, 3000));

    g_assert_true (peer_saw (peer_a, "only-for-a"));
    g_assert_false (peer_saw (peer_a, "only-for-b"));
    g_assert_true (peer_saw (peer_b, "only-for-b"));
    g_assert_false (peer_saw (peer_b, "only-for-a"));

    mcp_transport_disconnect_async (MCP_TRANSPORT (client_a), NULL, NULL, NULL);
    mcp_transport_disconnect_async (MCP_TRANSPORT (client_b), NULL, NULL, NULL);
    mcp_transport_disconnect_async (MCP_TRANSPORT (server_a), NULL, NULL, NULL);
    mcp_transport_disconnect_async (MCP_TRANSPORT (server_b), NULL, NULL, NULL);
    pump (100);

    peer_free (peer_a);
    peer_free (peer_b);
}

/* ========================================================================== */
/* A server whose SSE lines end in CRLF                                       */
/* ========================================================================== */

/*
 * The SSE reader left GDataInputStream at its default LF newline type, so
 * a CRLF server's blank separator line arrives as "\r" -- length 1, not
 * 0 -- and the "dispatch the event" branch is never taken.  No event ever
 * fires and the accumulated data grows for the life of the connection.
 * mcp-stdio-transport.c sets the newline type; this one did not.
 */
typedef struct
{
    SoupServer *server;
    guint       port;
} CrlfServer;

static void
crlf_sse_handler (SoupServer        *server,
                  SoupServerMessage *msg,
                  const char        *path,
                  GHashTable        *query,
                  gpointer           user_data)
{
    SoupMessageHeaders *headers;
    SoupMessageBody *body;
    const gchar *event =
        "event: message\r\n"
        "data: {\"jsonrpc\":\"2.0\",\"method\":\"crlf\"}\r\n"
        "\r\n";

    (void) server;
    (void) path;
    (void) query;
    (void) user_data;

    headers = soup_server_message_get_response_headers (msg);
    soup_message_headers_replace (headers, "Content-Type", "text/event-stream");
    soup_message_headers_set_encoding (headers, SOUP_ENCODING_CHUNKED);
    soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);

    body = soup_server_message_get_response_body (msg);
    soup_message_body_append (body, SOUP_MEMORY_COPY, event, strlen (event));
}

static void
test_crlf_line_endings (void)
{
    g_autoptr(SoupServer) raw = NULL;
    g_autoptr(McpHttpTransport) client = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree gchar *url = NULL;
    Peer *peer = peer_new ();
    GSList *uris;
    guint port;

    raw = soup_server_new (NULL, NULL);
    soup_server_add_handler (raw, "/sse", crlf_sse_handler, NULL, NULL);
    g_assert_true (soup_server_listen_local (raw, 0, 0, &error));
    g_assert_no_error (error);

    uris = soup_server_get_uris (raw);
    g_assert_nonnull (uris);
    port = (guint) g_uri_get_port (uris->data);
    g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);

    url = g_strdup_printf ("http://127.0.0.1:%u", port);
    client = mcp_http_transport_new (url);
    mcp_http_transport_set_sse_endpoint (client, "/sse");
    g_signal_connect (client, "message-received", G_CALLBACK (on_peer_message), peer);
    g_signal_connect (client, "error", G_CALLBACK (on_peer_error), peer);

    g_assert_true (connect_transport (MCP_TRANSPORT (client), 2000));
    g_assert_true (pump_until_frames (peer, 1, 3000));
    g_assert_true (peer_saw (peer, "\"crlf\""));

    mcp_transport_disconnect_async (MCP_TRANSPORT (client), NULL, NULL, NULL);
    pump (100);
    soup_server_disconnect (raw);

    peer_free (peer);
}

/* ========================================================================== */
/* Two POSTs in flight at once                                                */
/* ========================================================================== */

typedef struct
{
    gboolean finished;
    gboolean ok;
    GError  *error;
} PostOutcome;

static void
on_post_done (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
    PostOutcome *outcome = user_data;

    outcome->ok = mcp_transport_send_message_finish (MCP_TRANSPORT (source),
                                                     result, &outcome->error);
    outcome->finished = TRUE;
}

/*
 * The per-request SoupMessage was parked on the *shared* SoupSession with
 * g_object_set_data_full ("current-message"), and both POSTs are started
 * back to back before either callback runs -- so the first request's
 * callback read the second request's status and body, and the first
 * SoupMessage was unreffed while libsoup still held it.
 *
 * Two responses that differ is what makes that visible: a server that
 * answers both the same way cannot tell the versions apart, which is why
 * this drives a raw SoupServer that refuses one POST and accepts the
 * other.
 */
static void
discriminating_post_handler (SoupServer        *server,
                             SoupServerMessage *msg,
                             const char        *path,
                             GHashTable        *query,
                             gpointer           user_data)
{
    g_autoptr(GBytes) body = NULL;
    const gchar *data;
    gsize len;

    (void) server;
    (void) path;
    (void) query;
    (void) user_data;

    body = soup_message_body_flatten (soup_server_message_get_request_body (msg));
    data = g_bytes_get_data (body, &len);

    if (data != NULL && g_strstr_len (data, (gssize) len, "post-one") != NULL)
    {
        soup_server_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR, NULL);
        return;
    }

    soup_server_message_set_status (msg, SOUP_STATUS_ACCEPTED, NULL);
}

static void
test_overlapping_posts (void)
{
    g_autoptr(SoupServer) raw = NULL;
    g_autoptr(McpHttpTransport) client = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree gchar *url = NULL;
    g_autoptr(JsonNode) one = make_notification ("post-one");
    g_autoptr(JsonNode) two = make_notification ("post-two");
    PostOutcome outcome_one = { FALSE, FALSE, NULL };
    PostOutcome outcome_two = { FALSE, FALSE, NULL };
    GSList *uris;
    guint port;
    gint64 deadline;

    raw = soup_server_new (NULL, NULL);
    soup_server_add_handler (raw, "/", discriminating_post_handler, NULL, NULL);
    soup_server_add_handler (raw, "/sse", crlf_sse_handler, NULL, NULL);
    g_assert_true (soup_server_listen_local (raw, 0, 0, &error));
    g_assert_no_error (error);

    uris = soup_server_get_uris (raw);
    g_assert_nonnull (uris);
    port = (guint) g_uri_get_port (uris->data);
    g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);

    url = g_strdup_printf ("http://127.0.0.1:%u", port);
    client = mcp_http_transport_new (url);
    mcp_http_transport_set_sse_endpoint (client, "/sse");
    g_assert_true (connect_transport (MCP_TRANSPORT (client), 2000));
    pump (100);

    /* Both start before either callback runs, which is the whole point:
     * whatever the shared session was carrying is already the second
     * request's by the time the first is answered. */
    mcp_transport_send_message_async (MCP_TRANSPORT (client), one, NULL,
                                      on_post_done, &outcome_one);
    mcp_transport_send_message_async (MCP_TRANSPORT (client), two, NULL,
                                      on_post_done, &outcome_two);

    deadline = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;
    while ((!outcome_one.finished || !outcome_two.finished)
           && g_get_monotonic_time () < deadline)
    {
        if (!g_main_context_iteration (NULL, FALSE))
        {
            g_usleep (500);
        }
    }

    g_assert_true (outcome_one.finished);
    g_assert_true (outcome_two.finished);

    /* The refused POST must report its own 500, not the other one's 202. */
    g_assert_false (outcome_one.ok);
    g_assert_error (outcome_one.error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (outcome_one.error->message, "500"));

    g_assert_true (outcome_two.ok);
    g_assert_no_error (outcome_two.error);

    mcp_transport_disconnect_async (MCP_TRANSPORT (client), NULL, NULL, NULL);
    pump (100);
    soup_server_disconnect (raw);

    g_clear_error (&outcome_one.error);
    g_clear_error (&outcome_two.error);
}

/* ========================================================================== */
/* A named host is the address it binds, not a comment                        */
/* ========================================================================== */

/*
 * Both server transports listened on 127.0.0.1 for "localhost" and on
 * *everything* for anything else, with a comment saying the handler would
 * filter -- and no filter was ever written.  require-auth defaults to
 * FALSE, so naming an interface published the server to every network the
 * machine is on.
 */
static gboolean
can_connect (const gchar *address,
             guint        port)
{
    g_autoptr(GSocketClient) socket_client = g_socket_client_new ();
    g_autoptr(GSocketConnection) connection = NULL;
    g_autoptr(GError) error = NULL;

    g_socket_client_set_timeout (socket_client, 2);
    connection = g_socket_client_connect_to_host (socket_client, address, port,
                                                  NULL, &error);

    return connection != NULL;
}

static void
test_named_host_binds_only_that_host (void)
{
    g_autoptr(McpHttpServerTransport) server = NULL;
    guint port;

    /* 127.0.0.2 is inside the loopback network, so this stays local. */
    server = mcp_http_server_transport_new_full ("127.0.0.2", 0);
    g_assert_true (connect_transport (MCP_TRANSPORT (server), 2000));

    port = mcp_http_server_transport_get_actual_port (server);
    g_assert_cmpuint (port, >, 0);

    g_assert_true (can_connect ("127.0.0.2", port));
    g_assert_false (can_connect ("127.0.0.1", port));

    mcp_transport_disconnect_async (MCP_TRANSPORT (server), NULL, NULL, NULL);
    pump (100);
}

static void
test_websocket_named_host_binds_only_that_host (void)
{
    g_autoptr(McpWebSocketServerTransport) server = NULL;
    guint port;

    server = mcp_websocket_server_transport_new_full ("127.0.0.2", 0, NULL);
    g_assert_true (connect_transport (MCP_TRANSPORT (server), 2000));

    port = mcp_websocket_server_transport_get_actual_port (server);
    g_assert_cmpuint (port, >, 0);

    g_assert_true (can_connect ("127.0.0.2", port));
    g_assert_false (can_connect ("127.0.0.1", port));

    mcp_transport_disconnect_async (MCP_TRANSPORT (server), NULL, NULL, NULL);
    pump (100);
}

/* ========================================================================== */
/* Main                                                                       */
/* ========================================================================== */

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/mcp/http-sse/server-sends-more-than-one-event",
                     test_server_sends_more_than_one_event);
    g_test_add_func ("/mcp/http-sse/two-clients-do-not-share-event-state",
                     test_two_clients_do_not_share_event_state);
    g_test_add_func ("/mcp/http-sse/crlf-line-endings",
                     test_crlf_line_endings);
    g_test_add_func ("/mcp/http-sse/overlapping-posts",
                     test_overlapping_posts);
    g_test_add_func ("/mcp/http-server-transport/named-host-binds-only-that-host",
                     test_named_host_binds_only_that_host);
    g_test_add_func ("/mcp/websocket-server-transport/named-host-binds-only-that-host",
                     test_websocket_named_host_binds_only_that_host);

    return g_test_run ();
}
