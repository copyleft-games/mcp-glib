/*
 * test-client-handshake.c - What a failing handshake does to the connect task
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * mcp_client_connect_async()'s GTask is kept in two places at once: on
 * the client as ->connect_task, and in the session's pending-request
 * table, because it is also the task for the outbound initialize.  A
 * transport that fails reaches both -- the "error" signal returns the
 * first, and the state change that follows runs
 * mcp_session_cancel_all_pending_requests() over the second.
 */

#define MCP_COMPILATION
#include "mcp.h"
#include "mcp-mux-transport.h"
#undef MCP_COMPILATION

#include <string.h>

typedef struct
{
    guint    calls;
    GError  *error;
} ConnectOutcome;

static void
on_connected (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
    ConnectOutcome *outcome = user_data;

    outcome->calls++;
    mcp_client_connect_finish (MCP_CLIENT (source), result, &outcome->error);
}

static void
pump (GMainContext *context,
      guint         milliseconds)
{
    gint64 deadline = g_get_monotonic_time () + milliseconds * 1000;

    while (g_get_monotonic_time () < deadline)
    {
        if (!g_main_context_iteration (context, FALSE))
        {
            g_usleep (500);
        }
    }
}

/*
 * A transport error during the handshake, followed by the disconnect that
 * every real transport reports alongside it.  Without the fix the second
 * path returns the same GTask again, which GLib refuses with
 * "g_task_return_error: assertion '!task->ever_returned' failed" -- fatal
 * under GTest, and in production a critical plus a connect callback whose
 * reason is whichever return happened to be second.
 */
static void
test_failed_handshake_returns_the_task_once (void)
{
    g_autoptr(GMainContext) context = g_main_context_new ();
    g_autoptr(McpMuxTransport) transport = NULL;
    g_autoptr(McpClient) client = NULL;
    g_autoptr(GError) failure = NULL;
    ConnectOutcome outcome = { 0, NULL };

    g_main_context_push_thread_default (context);

    transport = mcp_mux_transport_new (context);
    mcp_mux_transport_set_connected (transport, TRUE);

    client = mcp_client_new ("handshake-test", "1.0.0");
    mcp_client_set_transport (client, MCP_TRANSPORT (transport));
    mcp_client_connect_async (client, NULL, on_connected, &outcome);

    pump (context, 100);

    /* The initialize is out and unanswered: the connect task is in the
     * pending table and on the client at the same time. */
    g_assert_cmpuint (outcome.calls, ==, 0);

    failure = g_error_new (MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR, "carrier died");
    mcp_mux_transport_emit_error_from_host (transport, failure);
    mcp_mux_transport_set_connected (transport, FALSE);

    pump (context, 200);

    g_assert_cmpuint (outcome.calls, ==, 1);
    g_assert_nonnull (outcome.error);

    g_clear_error (&outcome.error);
    g_main_context_pop_thread_default (context);
}

/* ========================================================================== */
/* A hostile server                                                           */
/* ========================================================================== */

typedef struct
{
    GMainContext    *context;
    McpMuxTransport *transport;
    McpClient       *client;
    GPtrArray       *sent;      /* gchar * — serialised outbound frames */
} HostileFixture;

static void
capture_send (McpMuxTransport *transport,
              JsonNode        *frame,
              gpointer         user_data)
{
    HostileFixture *fixture = user_data;
    g_autoptr(JsonGenerator) generator = json_generator_new ();

    (void) transport;

    json_generator_set_root (generator, frame);
    g_ptr_array_add (fixture->sent, json_generator_to_data (generator, NULL));
}

static void
hostile_set_up (HostileFixture *fixture,
                gconstpointer   user_data)
{
    (void) user_data;

    fixture->context = g_main_context_new ();
    g_main_context_push_thread_default (fixture->context);

    fixture->sent = g_ptr_array_new_with_free_func (g_free);
    fixture->transport = mcp_mux_transport_new (fixture->context);
    mcp_mux_transport_set_send_callback (fixture->transport, capture_send,
                                         fixture, NULL);
    mcp_mux_transport_set_connected (fixture->transport, TRUE);

    fixture->client = mcp_client_new ("hostile-test", "1.0.0");
    mcp_client_set_transport (fixture->client, MCP_TRANSPORT (fixture->transport));
}

static void
hostile_tear_down (HostileFixture *fixture,
                   gconstpointer   user_data)
{
    (void) user_data;

    mcp_mux_transport_set_send_callback (fixture->transport, NULL, NULL, NULL);
    g_clear_object (&fixture->client);
    g_clear_object (&fixture->transport);
    g_ptr_array_unref (fixture->sent);

    g_main_context_pop_thread_default (fixture->context);
    g_main_context_unref (fixture->context);
}

static void
hostile_dispatch (HostileFixture *fixture,
                  const gchar    *json)
{
    g_autoptr(JsonParser) parser = json_parser_new ();

    g_assert_true (json_parser_load_from_data (parser, json, -1, NULL));
    mcp_mux_transport_dispatch_frame (fixture->transport,
                                      json_parser_get_root (parser));
    pump (fixture->context, 100);
}

/*
 * Every field below is present and of the wrong type.  Each one used to
 * pass a json_object_has_member() gate and come back NULL from the typed
 * reader, which is a precondition inside the constructor it was on its
 * way to -- fatal under GTest, and a critical plus a wrong answer in
 * production.  A hostile server should get a client that ignores it, not
 * one that dies.
 */
static void
test_hostile_initialize_response (HostileFixture *fixture,
                                  gconstpointer   user_data)
{
    (void) user_data;

    mcp_client_connect_async (fixture->client, NULL, NULL, NULL);
    pump (fixture->context, 100);

    hostile_dispatch (fixture,
        "{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":{"
        "\"protocolVersion\":7,"
        "\"capabilities\":\"not-an-object\","
        "\"serverInfo\":{\"name\":123,\"version\":[]},"
        "\"instructions\":{\"a\":1}}}");

    /* The client survives, and reads nothing it was not given. */
    g_assert_null (mcp_client_get_server_instructions (fixture->client));
}

static void
test_hostile_tools_list_response (HostileFixture *fixture,
                                  gconstpointer   user_data)
{
    (void) user_data;

    hostile_dispatch (fixture,
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/message\","
        "\"params\":{\"level\":5,\"logger\":[],\"data\":1}}");

    hostile_dispatch (fixture,
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/resources/updated\","
        "\"params\":{\"uri\":{\"not\":\"a string\"}}}");

    /* A notification whose params are all the wrong type is ignored, not
     * dereferenced. */
    g_assert_true (MCP_IS_CLIENT (fixture->client));
}

static void
test_hostile_sampling_request (HostileFixture *fixture,
                               gconstpointer   user_data)
{
    (void) user_data;

    hostile_dispatch (fixture,
        "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"sampling/createMessage\","
        "\"params\":{\"messages\":\"not-an-array\","
        "\"systemPrompt\":42,\"maxTokens\":\"lots\"}}");

    /* An error response, not a crash. */
    g_assert_cmpuint (fixture->sent->len, >, 0);
    g_assert_nonnull (strstr (g_ptr_array_index (fixture->sent,
                                                 fixture->sent->len - 1),
                              "error"));
}

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/mcp/client/failed-handshake-returns-the-task-once",
                     test_failed_handshake_returns_the_task_once);

    g_test_add ("/mcp/client/hostile-initialize-response", HostileFixture, NULL,
                hostile_set_up, test_hostile_initialize_response, hostile_tear_down);
    g_test_add ("/mcp/client/hostile-notifications", HostileFixture, NULL,
                hostile_set_up, test_hostile_tools_list_response, hostile_tear_down);
    g_test_add ("/mcp/client/hostile-sampling-request", HostileFixture, NULL,
                hostile_set_up, test_hostile_sampling_request, hostile_tear_down);

    return g_test_run ();
}
