/*
 * test-redirects.c - A credential never follows a redirect
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * libsoup follows 301/302/303/307/308 by default and copies the request
 * headers onto the new message, so a transport's bearer token went
 * wherever a Location header pointed -- another host included -- and a
 * 307/308 resent the JSON-RPC body with it.
 *
 * The first half drives the decision (mcp-redirect-util.h) directly, so
 * every branch is reachable without a server.  The second half points
 * each transport at a loopback server that redirects to a second
 * loopback server -- another port is another origin -- and checks the
 * second one never hears from it.  Nothing leaves 127.0.0.1.
 */

#define MCP_COMPILATION
#include "mcp.h"
#include "mcp-http-transport.h"
#include "mcp-websocket-transport.h"
#include "mcp-redirect-util.h"
#undef MCP_COMPILATION

#include <libsoup/soup.h>
#include <string.h>

/* ========================================================================== */
/* The decision                                                               */
/* ========================================================================== */

/* Only the statuses libsoup would have followed are refusals. */
static void
test_refusal_only_for_followable_statuses (void)
{
    static const guint not_redirects[] = { 200, 204, 300, 304, 305, 400, 401, 404, 500 };
    static const guint redirects[] = { 301, 302, 303, 307, 308 };
    guint i;

    for (i = 0; i < G_N_ELEMENTS (not_redirects); i++)
    {
        GError *refusal;

        refusal = mcp_redirect_refusal ("https://mcp.example/mcp",
                                        not_redirects[i],
                                        "https://elsewhere.example/");
        g_assert_null (refusal);
    }

    for (i = 0; i < G_N_ELEMENTS (redirects); i++)
    {
        g_autoptr(GError) refusal = NULL;

        refusal = mcp_redirect_refusal ("https://mcp.example/mcp",
                                        redirects[i],
                                        "https://elsewhere.example/");
        g_assert_error (refusal, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    }
}

/*
 * Another host is named by its origin only: the path and query of a
 * Location can carry a code or a token, and this message reaches logs.
 */
static void
test_refusal_cross_origin_names_only_the_origin (void)
{
    g_autoptr(GError) refusal = NULL;

    refusal = mcp_redirect_refusal ("https://mcp.example/mcp?key=mine",
                                    307,
                                    "https://evil.example:8443/steal?code=secret");
    g_assert_error (refusal, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (refusal->message, "https://mcp.example"));
    g_assert_nonnull (strstr (refusal->message, "https://evil.example:8443"));
    g_assert_nonnull (strstr (refusal->message, "307"));
    g_assert_null (strstr (refusal->message, "steal"));
    g_assert_null (strstr (refusal->message, "secret"));
    g_assert_null (strstr (refusal->message, "mine"));
}

/* A scheme change is a different origin, and the usual http->https case. */
static void
test_refusal_scheme_change_is_another_origin (void)
{
    g_autoptr(GError) refusal = NULL;

    refusal = mcp_redirect_refusal ("http://mcp.example/mcp", 301,
                                    "https://mcp.example/mcp");
    g_assert_error (refusal, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (refusal->message,
                              "configure that address instead"));
    g_assert_nonnull (strstr (refusal->message, "https://mcp.example"));
}

/* A relative Location resolves against the request, and names the path. */
static void
test_refusal_same_origin_names_the_path (void)
{
    g_autoptr(GError) refusal = NULL;

    refusal = mcp_redirect_refusal ("https://mcp.example/mcp", 308, "/mcp/");
    g_assert_error (refusal, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (refusal->message, "/mcp/"));
    g_assert_nonnull (strstr (refusal->message, "same server"));
}

static void
test_refusal_without_a_usable_location (void)
{
    g_autoptr(GError) missing = NULL;
    g_autoptr(GError) empty = NULL;
    g_autoptr(GError) broken = NULL;

    missing = mcp_redirect_refusal ("https://mcp.example/mcp", 302, NULL);
    g_assert_error (missing, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (missing->message, "no Location header"));

    empty = mcp_redirect_refusal ("https://mcp.example/mcp", 302, "");
    g_assert_error (empty, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (empty->message, "no Location header"));

    /* No base to resolve against, and not absolute either. */
    broken = mcp_redirect_refusal (NULL, 302, "not a uri");
    g_assert_error (broken, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (broken->message, "does not parse"));
    g_assert_nonnull (strstr (broken->message, "the server"));
}

/* g_uri_join() brackets an IPv6 literal; a printf would not. */
static void
test_refusal_brackets_ipv6 (void)
{
    g_autoptr(GError) refusal = NULL;

    refusal = mcp_redirect_refusal ("http://[::1]:8080/mcp", 302,
                                    "http://[fe80::1]:9090/x");
    g_assert_error (refusal, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (refusal->message, "http://[::1]:8080"));
    g_assert_nonnull (strstr (refusal->message, "http://[fe80::1]:9090"));
}

/* A message that was allowed to follow redirects is never refused. */
static void
test_check_ignores_messages_that_may_follow (void)
{
    g_autoptr(SoupMessage) message = NULL;
    GError *refusal;

    message = soup_message_new ("GET", "http://127.0.0.1:1/");
    refusal = mcp_redirect_check (message);
    g_assert_null (refusal);
    g_assert_null (mcp_redirect_check (NULL));
}

/*
 * The constructor every transport goes through decides before any header
 * is set: NEVER is the flag on the message it returns, FOLLOW is libsoup's
 * default, and zero -- a forgotten argument -- is NEVER.
 */
static void
test_message_new_applies_the_policy (void)
{
    g_autoptr(SoupMessage) never = NULL;
    g_autoptr(SoupMessage) follow = NULL;
    g_autoptr(SoupMessage) zero = NULL;

    g_assert_cmpint (MCP_REDIRECT_NEVER, ==, 0);

    never = mcp_redirect_message_new ("POST", "http://127.0.0.1:1/mcp",
                                      MCP_REDIRECT_NEVER);
    g_assert_nonnull (never);
    g_assert_cmpstr (soup_message_get_method (never), ==, "POST");
    g_assert_true ((soup_message_get_flags (never) & SOUP_MESSAGE_NO_REDIRECT) != 0);

    follow = mcp_redirect_message_new ("GET", "http://127.0.0.1:1/sse",
                                       MCP_REDIRECT_FOLLOW);
    g_assert_nonnull (follow);
    g_assert_true ((soup_message_get_flags (follow) & SOUP_MESSAGE_NO_REDIRECT) == 0);
    g_assert_null (mcp_redirect_check (follow));

    zero = mcp_redirect_message_new ("GET", "http://127.0.0.1:1/",
                                     (McpRedirectPolicy) 0);
    g_assert_true ((soup_message_get_flags (zero) & SOUP_MESSAGE_NO_REDIRECT) != 0);

    /* An unparseable URL is NULL, as from soup_message_new(). */
    g_assert_null (mcp_redirect_message_new ("GET", "not a url",
                                             MCP_REDIRECT_FOLLOW));
}

/* ========================================================================== */
/* Two loopback servers: one redirects, the other counts who arrives          */
/* ========================================================================== */

typedef struct
{
    SoupServer *origin;      /* the server the transport is pointed at */
    SoupServer *elsewhere;   /* where origin's redirects point */
    gchar      *origin_url;
    gchar      *elsewhere_url;
    guint       redirect_status;
    const gchar *sse_session_id;   /* what origin's /sse hands out, if any */
    guint       elsewhere_hits;
    gboolean    elsewhere_saw_authorization;
    gboolean    elsewhere_saw_session_id;
} Fixture;

static guint
listen_loopback (SoupServer *server)
{
    g_autoptr(GError) error = NULL;
    GSList *uris;
    guint port;

    g_assert_true (soup_server_listen_local (server, 0,
                                             SOUP_SERVER_LISTEN_IPV4_ONLY,
                                             &error));
    g_assert_no_error (error);

    uris = soup_server_get_uris (server);
    g_assert_nonnull (uris);
    port = (guint) g_uri_get_port (uris->data);
    g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);

    return port;
}

/* Every path on origin answers with a redirect to the same path elsewhere. */
static void
redirecting_handler (SoupServer        *server,
                     SoupServerMessage *msg,
                     const char        *path,
                     GHashTable        *query,
                     gpointer           user_data)
{
    Fixture *fixture = user_data;
    g_autofree gchar *target = NULL;

    (void) server;
    (void) query;

    target = g_strconcat (fixture->elsewhere_url, path, NULL);
    soup_server_message_set_redirect (msg, fixture->redirect_status, target);
}

/*
 * A normal, open SSE stream, so the transport reaches CONNECTED -- and,
 * when the test asks, hands out a session id the way a server does.
 */
static void
sse_handler (SoupServer        *server,
             SoupServerMessage *msg,
             const char        *path,
             GHashTable        *query,
             gpointer           user_data)
{
    Fixture *fixture = user_data;
    SoupMessageHeaders *headers;

    (void) server;
    (void) path;
    (void) query;

    headers = soup_server_message_get_response_headers (msg);
    if (fixture->sse_session_id != NULL)
    {
        soup_message_headers_replace (headers, "Mcp-Session-Id",
                                      fixture->sse_session_id);
    }
    soup_message_headers_replace (headers, "Content-Type", "text/event-stream");
    soup_message_headers_set_encoding (headers, SOUP_ENCODING_CHUNKED);
    soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
    soup_message_body_append (soup_server_message_get_response_body (msg),
                              SOUP_MEMORY_STATIC, ": open\n\n", 9);
}

static void
counting_handler (SoupServer        *server,
                  SoupServerMessage *msg,
                  const char        *path,
                  GHashTable        *query,
                  gpointer           user_data)
{
    Fixture *fixture = user_data;
    SoupMessageHeaders *headers;

    (void) server;
    (void) path;
    (void) query;

    headers = soup_server_message_get_request_headers (msg);
    fixture->elsewhere_hits++;

    if (soup_message_headers_get_one (headers, "Authorization") != NULL)
    {
        fixture->elsewhere_saw_authorization = TRUE;
    }

    if (soup_message_headers_get_one (headers, "Mcp-Session-Id") != NULL)
    {
        fixture->elsewhere_saw_session_id = TRUE;
    }

    soup_server_message_set_status (msg, SOUP_STATUS_ACCEPTED, NULL);
}

static void
fixture_set_up (Fixture       *fixture,
                gconstpointer  user_data)
{
    (void) user_data;

    memset (fixture, 0, sizeof (*fixture));
    fixture->redirect_status = SOUP_STATUS_TEMPORARY_REDIRECT;

    fixture->elsewhere = soup_server_new (NULL, NULL);
    soup_server_add_handler (fixture->elsewhere, NULL, counting_handler,
                             fixture, NULL);
    fixture->elsewhere_url = g_strdup_printf ("http://127.0.0.1:%u",
                                              listen_loopback (fixture->elsewhere));

    fixture->origin = soup_server_new (NULL, NULL);
    soup_server_add_handler (fixture->origin, NULL, redirecting_handler,
                             fixture, NULL);
    soup_server_add_handler (fixture->origin, "/sse", sse_handler,
                             fixture, NULL);
    fixture->origin_url = g_strdup_printf ("http://127.0.0.1:%u",
                                           listen_loopback (fixture->origin));
}

static void
fixture_tear_down (Fixture       *fixture,
                   gconstpointer  user_data)
{
    (void) user_data;

    soup_server_disconnect (fixture->origin);
    soup_server_disconnect (fixture->elsewhere);
    g_clear_object (&fixture->origin);
    g_clear_object (&fixture->elsewhere);
    g_free (fixture->origin_url);
    g_free (fixture->elsewhere_url);
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

static void
on_transport_error (McpTransport *transport,
                    GError       *error,
                    gpointer      user_data)
{
    GError **last = user_data;

    (void) transport;

    g_clear_error (last);
    *last = g_error_copy (error);
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

typedef struct
{
    gboolean finished;
    gboolean ok;
    GError  *error;
} Outcome;

static void
on_post_done (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
    Outcome *outcome = user_data;

    outcome->ok = mcp_transport_send_message_finish (MCP_TRANSPORT (source),
                                                     result, &outcome->error);
    outcome->finished = TRUE;
}

static void
on_connect_done (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    Outcome *outcome = user_data;

    outcome->ok = mcp_transport_connect_finish (MCP_TRANSPORT (source),
                                                result, &outcome->error);
    outcome->finished = TRUE;
}

static void
pump_until_finished (Outcome *outcome,
                     guint    milliseconds)
{
    gint64 deadline = g_get_monotonic_time () + milliseconds * 1000;

    while (!outcome->finished && g_get_monotonic_time () < deadline)
    {
        if (!g_main_context_iteration (NULL, FALSE))
        {
            g_usleep (500);
        }
    }

    g_assert_true (outcome->finished);
}

/* POST one notification over @client, with its SSE stream on origin. */
static void
post_once (McpHttpTransport *client,
           Outcome          *outcome)
{
    g_autoptr(JsonNode) note = make_notification ("notifications/redirect-test");

    mcp_http_transport_set_sse_endpoint (client, "/sse");
    mcp_http_transport_set_post_endpoint (client, "/mcp");
    mcp_transport_connect_async (MCP_TRANSPORT (client), NULL, NULL, NULL);
    pump (100);

    mcp_transport_send_message_async (MCP_TRANSPORT (client), note, NULL,
                                      on_post_done, outcome);
    pump_until_finished (outcome, 5000);

    mcp_transport_disconnect_async (MCP_TRANSPORT (client), NULL, NULL, NULL);
    pump (50);
}

/*
 * The POST that carried the token -- and, on a 307, its body -- to
 * whatever host the Location named.
 */
static void
test_http_post_with_token_does_not_follow (Fixture       *fixture,
                                           gconstpointer  user_data)
{
    g_autoptr(McpHttpTransport) client = NULL;
    Outcome outcome = { FALSE, FALSE, NULL };

    (void) user_data;

    client = mcp_http_transport_new (fixture->origin_url);
    mcp_http_transport_set_auth_token (client, "s3cr3t-token");
    post_once (client, &outcome);

    g_assert_false (outcome.ok);
    g_assert_error (outcome.error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (outcome.error->message, fixture->elsewhere_url));
    g_assert_null (strstr (outcome.error->message, "s3cr3t"));
    g_assert_cmpuint (fixture->elsewhere_hits, ==, 0);
    g_assert_false (fixture->elsewhere_saw_authorization);

    g_clear_error (&outcome.error);
}

/* A session id lets its holder speak inside the session: a credential. */
static void
test_http_post_with_session_id_does_not_follow (Fixture       *fixture,
                                                gconstpointer  user_data)
{
    g_autoptr(McpHttpTransport) client = NULL;
    Outcome outcome = { FALSE, FALSE, NULL };

    (void) user_data;

    /*
     * A 302 is the live leak: libsoup turns the POST into a GET and
     * follows it, and even 3.6.6, which drops Authorization for another
     * host, keeps every other header.
     */
    fixture->redirect_status = SOUP_STATUS_FOUND;
    fixture->sse_session_id = "session-1234";
    client = mcp_http_transport_new (fixture->origin_url);

    /* No token: the session id the stream handed out is the only secret. */
    post_once (client, &outcome);

    /* "credential" is only said when the session id was on the POST. */
    g_assert_false (outcome.ok);
    g_assert_error (outcome.error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (outcome.error->message, "credential"));
    g_assert_cmpuint (fixture->elsewhere_hits, ==, 0);
    g_assert_false (fixture->elsewhere_saw_session_id);

    g_clear_error (&outcome.error);
}

/*
 * With nothing to leak, a redirected POST is still not delivered:
 * libsoup does not follow a 307 for one, and a 302 becomes a GET with
 * no body.  Both used to be reported as success.
 */
static void
test_http_post_without_credentials_reports_the_redirect (Fixture       *fixture,
                                                         gconstpointer  user_data)
{
    static const guint statuses[] = { 302, 307 };
    guint i;

    (void) user_data;

    for (i = 0; i < G_N_ELEMENTS (statuses); i++)
    {
        g_autoptr(McpHttpTransport) client = NULL;
        Outcome outcome = { FALSE, FALSE, NULL };

        fixture->redirect_status = statuses[i];
        client = mcp_http_transport_new (fixture->origin_url);
        post_once (client, &outcome);

        g_assert_false (outcome.ok);
        g_assert_error (outcome.error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
        g_assert_nonnull (strstr (outcome.error->message, "not delivered"));
        g_assert_cmpuint (fixture->elsewhere_hits, ==, 0);

        g_clear_error (&outcome.error);
    }
}

/* Nothing to leak, so nothing changes: an anonymous SSE GET still follows. */
static void
test_http_sse_without_credentials_still_follows (Fixture       *fixture,
                                                 gconstpointer  user_data)
{
    g_autoptr(McpHttpTransport) client = NULL;
    GError *last_error = NULL;

    (void) user_data;

    fixture->redirect_status = SOUP_STATUS_FOUND;
    client = mcp_http_transport_new (fixture->origin_url);
    mcp_http_transport_set_sse_endpoint (client, "/moved-sse");
    g_signal_connect (client, "error", G_CALLBACK (on_transport_error), &last_error);

    mcp_transport_connect_async (MCP_TRANSPORT (client), NULL, NULL, NULL);
    pump (300);

    /* elsewhere answers 202, not a stream -- but it was asked. */
    g_assert_cmpuint (fixture->elsewhere_hits, ==, 1);
    g_assert_error (last_error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (last_error->message, "202"));

    mcp_transport_disconnect_async (MCP_TRANSPORT (client), NULL, NULL, NULL);
    pump (50);
    g_clear_error (&last_error);
}

/* The SSE GET carried the same token, and gives up rather than retry. */
static void
test_http_sse_with_token_does_not_follow (Fixture       *fixture,
                                          gconstpointer  user_data)
{
    g_autoptr(McpHttpTransport) client = NULL;
    GError *last_error = NULL;

    (void) user_data;

    fixture->redirect_status = SOUP_STATUS_FOUND;
    client = mcp_http_transport_new (fixture->origin_url);
    mcp_http_transport_set_auth_token (client, "s3cr3t-token");
    mcp_http_transport_set_sse_endpoint (client, "/moved-sse");
    mcp_http_transport_set_reconnect_enabled (client, TRUE);
    g_signal_connect (client, "error", G_CALLBACK (on_transport_error), &last_error);

    mcp_transport_connect_async (MCP_TRANSPORT (client), NULL, NULL, NULL);
    pump (300);

    g_assert_error (last_error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (last_error->message, "302"));
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (client)), ==,
                     MCP_TRANSPORT_STATE_ERROR);
    g_assert_cmpuint (fixture->elsewhere_hits, ==, 0);

    mcp_transport_disconnect_async (MCP_TRANSPORT (client), NULL, NULL, NULL);
    pump (50);
    g_clear_error (&last_error);
}

/* The handshake carried the token; the refusal replaces "not a WebSocket". */
static void
test_websocket_with_token_does_not_follow (Fixture       *fixture,
                                           gconstpointer  user_data)
{
    g_autoptr(McpWebSocketTransport) client = NULL;
    g_autofree gchar *ws_url = NULL;
    GError *last_error = NULL;
    Outcome outcome = { FALSE, FALSE, NULL };

    (void) user_data;

    fixture->redirect_status = SOUP_STATUS_FOUND;
    ws_url = g_strdup_printf ("ws%s/ws", fixture->origin_url + strlen ("http"));
    client = mcp_websocket_transport_new (ws_url);
    mcp_websocket_transport_set_auth_token (client, "s3cr3t-token");
    mcp_websocket_transport_set_reconnect_enabled (client, TRUE);
    g_signal_connect (client, "error", G_CALLBACK (on_transport_error), &last_error);

    mcp_transport_connect_async (MCP_TRANSPORT (client), NULL,
                                 on_connect_done, &outcome);
    pump_until_finished (&outcome, 5000);
    pump (100);

    g_assert_false (outcome.ok);
    g_assert_error (outcome.error, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR);
    g_assert_nonnull (strstr (outcome.error->message, "redirect"));
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (client)), ==,
                     MCP_TRANSPORT_STATE_ERROR);
    g_assert_cmpuint (fixture->elsewhere_hits, ==, 0);
    g_assert_false (fixture->elsewhere_saw_authorization);

    mcp_transport_disconnect_async (MCP_TRANSPORT (client), NULL, NULL, NULL);
    pump (50);
    g_clear_error (&outcome.error);
    g_clear_error (&last_error);
}

gint
main (gint   argc,
      gchar *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/mcp/redirects/refusal/only-followable-statuses",
                     test_refusal_only_for_followable_statuses);
    g_test_add_func ("/mcp/redirects/refusal/cross-origin-names-only-the-origin",
                     test_refusal_cross_origin_names_only_the_origin);
    g_test_add_func ("/mcp/redirects/refusal/scheme-change-is-another-origin",
                     test_refusal_scheme_change_is_another_origin);
    g_test_add_func ("/mcp/redirects/refusal/same-origin-names-the-path",
                     test_refusal_same_origin_names_the_path);
    g_test_add_func ("/mcp/redirects/refusal/without-a-usable-location",
                     test_refusal_without_a_usable_location);
    g_test_add_func ("/mcp/redirects/refusal/brackets-ipv6",
                     test_refusal_brackets_ipv6);
    g_test_add_func ("/mcp/redirects/check/ignores-messages-that-may-follow",
                     test_check_ignores_messages_that_may_follow);
    g_test_add_func ("/mcp/redirects/message-new/applies-the-policy",
                     test_message_new_applies_the_policy);

    g_test_add ("/mcp/redirects/http/post-with-token-does-not-follow",
                Fixture, NULL, fixture_set_up,
                test_http_post_with_token_does_not_follow, fixture_tear_down);
    g_test_add ("/mcp/redirects/http/post-with-session-id-does-not-follow",
                Fixture, NULL, fixture_set_up,
                test_http_post_with_session_id_does_not_follow, fixture_tear_down);
    g_test_add ("/mcp/redirects/http/post-without-credentials-reports-the-redirect",
                Fixture, NULL, fixture_set_up,
                test_http_post_without_credentials_reports_the_redirect, fixture_tear_down);
    g_test_add ("/mcp/redirects/http/sse-without-credentials-still-follows",
                Fixture, NULL, fixture_set_up,
                test_http_sse_without_credentials_still_follows, fixture_tear_down);
    g_test_add ("/mcp/redirects/http/sse-with-token-does-not-follow",
                Fixture, NULL, fixture_set_up,
                test_http_sse_with_token_does_not_follow, fixture_tear_down);
    g_test_add ("/mcp/redirects/websocket/with-token-does-not-follow",
                Fixture, NULL, fixture_set_up,
                test_websocket_with_token_does_not_follow, fixture_tear_down);

    return g_test_run ();
}
