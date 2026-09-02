/*
 * test-stdio-transport.c - Tests for McpStdioTransport's read and write loops
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The stdio transport is the only transport with no test of its own, and
 * it is the one every MCP subprocess uses.  These tests drive it over a
 * real pipe pair, because the two defects they cover -- a callback that
 * outlives the object it was handed, and a line that accumulates without
 * a bound -- are both about what happens between two turns of the main
 * loop.
 */

#define MCP_COMPILATION
#include "mcp.h"
#undef MCP_COMPILATION

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* ========================================================================== */
/* Fixture: a transport whose input is a pipe we hold the write end of        */
/* ========================================================================== */

typedef struct
{
    McpStdioTransport *transport;
    gint               to_transport;    /* we write, the transport reads   */
    gint               from_transport;  /* we read, the transport writes   */
    GPtrArray         *received;        /* gchar * — serialised frames     */
    GError            *last_error;
} StdioFixture;

static void
on_message (McpTransport *transport,
            JsonNode     *message,
            gpointer      user_data)
{
    StdioFixture *fixture = user_data;
    g_autoptr(JsonGenerator) generator = json_generator_new ();

    (void) transport;

    json_generator_set_root (generator, message);
    g_ptr_array_add (fixture->received, json_generator_to_data (generator, NULL));
}

static void
on_error (McpTransport *transport,
          GError       *error,
          gpointer      user_data)
{
    StdioFixture *fixture = user_data;

    (void) transport;

    g_clear_error (&fixture->last_error);
    fixture->last_error = g_error_copy (error);
}

static void
stdio_fixture_set_up (StdioFixture  *fixture,
                      gconstpointer  user_data)
{
    gint in_pipe[2];
    gint out_pipe[2];
    g_autoptr(GInputStream) input = NULL;
    g_autoptr(GOutputStream) output = NULL;

    (void) user_data;

    g_assert_cmpint (pipe (in_pipe), ==, 0);
    g_assert_cmpint (pipe (out_pipe), ==, 0);

    /* Non-blocking, because these tests both feed the transport and turn
     * the loop it drains on: a blocking write into a full pipe would wait
     * for a reader that only runs when we return. */
    g_assert_cmpint (fcntl (in_pipe[1], F_SETFL, O_NONBLOCK), ==, 0);

    fixture->to_transport = in_pipe[1];
    fixture->from_transport = out_pipe[0];
    fixture->received = g_ptr_array_new_with_free_func (g_free);
    fixture->last_error = NULL;

    input = g_unix_input_stream_new (in_pipe[0], TRUE);
    output = g_unix_output_stream_new (out_pipe[1], TRUE);

    fixture->transport = mcp_stdio_transport_new_with_streams (input, output);

    g_signal_connect (fixture->transport, "message-received",
                      G_CALLBACK (on_message), fixture);
    g_signal_connect (fixture->transport, "error",
                      G_CALLBACK (on_error), fixture);

    mcp_transport_connect_async (MCP_TRANSPORT (fixture->transport),
                                 NULL, NULL, NULL);

    /* The read loop starts from an idle, so give it one turn. */
    while (g_main_context_iteration (NULL, FALSE))
    {
    }
}

static void
stdio_fixture_tear_down (StdioFixture  *fixture,
                         gconstpointer  user_data)
{
    (void) user_data;

    g_clear_object (&fixture->transport);

    if (fixture->to_transport >= 0)
    {
        close (fixture->to_transport);
    }
    if (fixture->from_transport >= 0)
    {
        close (fixture->from_transport);
    }

    g_ptr_array_unref (fixture->received);
    g_clear_error (&fixture->last_error);

    /* Let anything the unref cancelled complete before the next test. */
    while (g_main_context_iteration (NULL, FALSE))
    {
    }
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

/* Pump until @cond or a deadline; returns whether @cond came true. */
static gboolean
pump_until (StdioFixture *fixture,
            gboolean (*cond) (StdioFixture *),
            guint      milliseconds)
{
    gint64 deadline = g_get_monotonic_time () + milliseconds * 1000;

    while (!cond (fixture) && g_get_monotonic_time () < deadline)
    {
        if (!g_main_context_iteration (NULL, FALSE))
        {
            g_usleep (500);
        }
    }

    return cond (fixture);
}

/*
 * Push what fits, iterating the loop so the transport drains the pipe.
 * Returns the number of bytes accepted.
 */
static gsize
feed (StdioFixture *fixture,
      const gchar  *data,
      gsize         len)
{
    gsize offset = 0;
    gint64 deadline = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;

    while (offset < len && g_get_monotonic_time () < deadline)
    {
        gssize n;

        /* Once the transport has refused the stream it stops reading, so
         * the pipe fills and there is nothing left to wait for. */
        if (fixture->last_error != NULL)
        {
            break;
        }

        n = write (fixture->to_transport, data + offset, len - offset);

        if (n > 0)
        {
            offset += (gsize) n;
        }

        while (g_main_context_iteration (NULL, FALSE))
        {
        }

        if (n < 0)
        {
            g_usleep (500);
        }
    }

    return offset;
}

static gboolean
has_a_frame (StdioFixture *fixture)
{
    return fixture->received->len > 0;
}

static gboolean
has_an_error (StdioFixture *fixture)
{
    return fixture->last_error != NULL;
}

/* ========================================================================== */
/* Framing                                                                    */
/* ========================================================================== */

static void
test_reads_a_line (StdioFixture  *fixture,
                   gconstpointer  user_data)
{
    const gchar *frame = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}\n";

    (void) user_data;

    g_assert_cmpint (write (fixture->to_transport, frame, strlen (frame)), >, 0);
    g_assert_true (pump_until (fixture, has_a_frame, 2000));

    g_assert_cmpuint (fixture->received->len, ==, 1);
    g_assert_nonnull (strstr (g_ptr_array_index (fixture->received, 0), "\"ping\""));
}

/* A message split across two writes is still one message. */
static void
test_reads_a_line_split_across_reads (StdioFixture  *fixture,
                                      gconstpointer  user_data)
{
    (void) user_data;

    g_assert_cmpint (write (fixture->to_transport, "{\"jsonrpc\":\"2.0\",", 17), >, 0);
    pump (50);
    g_assert_cmpuint (fixture->received->len, ==, 0);

    g_assert_cmpint (write (fixture->to_transport, "\"method\":\"ping\"}\n", 17), >, 0);
    g_assert_true (pump_until (fixture, has_a_frame, 2000));

    g_assert_cmpuint (fixture->received->len, ==, 1);
}

/* Two messages in one write are two messages. */
static void
test_reads_two_lines_from_one_write (StdioFixture  *fixture,
                                     gconstpointer  user_data)
{
    const gchar *frames =
        "{\"jsonrpc\":\"2.0\",\"method\":\"a\"}\n"
        "{\"jsonrpc\":\"2.0\",\"method\":\"b\"}\n";

    (void) user_data;

    g_assert_cmpint (write (fixture->to_transport, frames, strlen (frames)), >, 0);
    g_assert_true (pump_until (fixture, has_a_frame, 2000));
    pump (50);

    g_assert_cmpuint (fixture->received->len, ==, 2);
}

static void
test_writes_a_line (StdioFixture  *fixture,
                    gconstpointer  user_data)
{
    g_autoptr(JsonBuilder) builder = json_builder_new ();
    g_autoptr(JsonNode) node = NULL;
    gchar buffer[256];
    gssize got;

    (void) user_data;

    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "jsonrpc");
    json_builder_add_string_value (builder, "2.0");
    json_builder_end_object (builder);
    node = json_builder_get_root (builder);

    mcp_transport_send_message_async (MCP_TRANSPORT (fixture->transport),
                                      node, NULL, NULL, NULL);
    pump (100);

    got = read (fixture->from_transport, buffer, sizeof buffer - 1);
    g_assert_cmpint (got, >, 0);
    buffer[got] = '\0';

    g_assert_nonnull (strstr (buffer, "\"jsonrpc\""));
    g_assert_cmpint (buffer[got - 1], ==, '\n');
}

/* ========================================================================== */
/* A line with no end must not be buffered without a bound                    */
/* ========================================================================== */

/*
 * g_data_input_stream_read_line_async() -- which this transport used --
 * *doubles its own buffer* until it finds a newline, whatever
 * g_buffered_input_stream_set_buffer_size() was set to.  Verified: 4 KiB
 * requested, 8 MiB reached after 8 MiB of newline-free input.  So a peer
 * that opens a connection and never sends a newline is a memory bomb, and
 * a limit checked after the read has already committed the memory is not
 * a limit.
 *
 * Without a cap this test does not assert-fail, it hangs until the
 * deadline: nothing ever reports anything.
 */
static void
test_unterminated_line_is_refused (StdioFixture  *fixture,
                                   gconstpointer  user_data)
{
    g_autofree gchar *chunk = g_malloc (64 * 1024);
    gsize written = 0;

    (void) user_data;

    memset (chunk, 'x', 64 * 1024);
    mcp_stdio_transport_set_max_line_bytes (fixture->transport, 256 * 1024);

    while (written < 4u * 1024u * 1024u && fixture->last_error == NULL)
    {
        written += feed (fixture, chunk, 64 * 1024);
    }

    g_assert_true (pump_until (fixture, has_an_error, 2000));
    g_assert_error (fixture->last_error, MCP_ERROR, MCP_ERROR_INVALID_REQUEST);
    g_assert_cmpint (mcp_transport_get_state (MCP_TRANSPORT (fixture->transport)),
                     ==, MCP_TRANSPORT_STATE_ERROR);
}

/* A frame that fits under the cap must still get through afterwards. */
static void
test_line_at_the_limit_is_accepted (StdioFixture  *fixture,
                                    gconstpointer  user_data)
{
    g_autofree gchar *frame = NULL;
    gsize len;
    gsize offset = 0;

    (void) user_data;

    mcp_stdio_transport_set_max_line_bytes (fixture->transport, 256 * 1024);

    /* ~100 KiB of payload, comfortably under the cap and comfortably over
     * one read chunk. */
    {
        g_autofree gchar *payload = g_strnfill (100 * 1024, 'y');
        frame = g_strdup_printf (
            "{\"jsonrpc\":\"2.0\",\"method\":\"big\",\"params\":{\"s\":\"%s\"}}\n",
            payload);
    }
    len = strlen (frame);
    offset = feed (fixture, frame, len);
    g_assert_cmpuint (offset, ==, len);

    g_assert_true (pump_until (fixture, has_a_frame, 2000));
    g_assert_no_error (fixture->last_error);
    g_assert_cmpuint (fixture->received->len, ==, 1);
}

/* ========================================================================== */
/* Teardown while a read is in flight                                         */
/* ========================================================================== */

/*
 * The read and write callbacks were handed a bare `self`.  Unreffing the
 * transport runs dispose, which frees it while the read is still armed,
 * and the very next turn of the main loop hands the freed pointer to
 * MCP_STDIO_TRANSPORT().  Nothing keeps the object alive and nothing
 * notices it is gone.
 *
 * Run in a subprocess: the failure is a GObject critical (fatal under
 * GTest) or a fault, and neither is something the parent can survive
 * asserting on directly.
 */
static void
teardown_child (void)
{
    gint fds[2];
    g_autoptr(GInputStream) input = NULL;
    g_autoptr(GOutputStream) output = NULL;
    McpStdioTransport *transport;
    gpointer weak = NULL;
    guint i;

    g_assert_cmpint (pipe (fds), ==, 0);

    input = g_unix_input_stream_new (fds[0], TRUE);
    output = g_unix_output_stream_new (fds[1], TRUE);

    transport = mcp_stdio_transport_new_with_streams (input, output);
    g_object_add_weak_pointer (G_OBJECT (transport), &weak);
    weak = transport;

    mcp_transport_connect_async (MCP_TRANSPORT (transport), NULL, NULL, NULL);
    while (g_main_context_iteration (NULL, FALSE))
    {
    }

    g_object_unref (transport);

    /* Unreffing must actually finalise: a fix that merely parks a strong
     * reference in the read callback turns the crash into a transport
     * that never goes away, and for a subprocess transport that is a
     * child nobody kills -- dispose is what sends it SIGTERM. */
    g_assert_null (weak);

    for (i = 0; i < 200; i++)
    {
        g_main_context_iteration (NULL, FALSE);
        g_usleep (500);
    }
}

static void
test_teardown_with_reads_in_flight (void)
{
    if (g_test_subprocess ())
    {
        teardown_child ();
        return;
    }

    g_test_trap_subprocess (NULL, 10 * G_USEC_PER_SEC, 0);
    g_test_trap_assert_passed ();
}

/*
 * The write callback had the same bare `self`.  A queued write keeps the
 * transport alive on its own (a GTask references its source object), so
 * this one asserts only that the callback survives the caller dropping
 * its reference.
 */
static void
teardown_write_child (void)
{
    gint fds[2];
    g_autoptr(GInputStream) input = NULL;
    g_autoptr(GOutputStream) output = NULL;
    g_autoptr(JsonBuilder) builder = json_builder_new ();
    g_autoptr(JsonNode) node = NULL;
    McpStdioTransport *transport;
    guint i;

    g_assert_cmpint (pipe (fds), ==, 0);

    input = g_unix_input_stream_new (fds[0], TRUE);
    output = g_unix_output_stream_new (fds[1], TRUE);

    transport = mcp_stdio_transport_new_with_streams (input, output);
    mcp_transport_connect_async (MCP_TRANSPORT (transport), NULL, NULL, NULL);
    while (g_main_context_iteration (NULL, FALSE))
    {
    }

    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "jsonrpc");
    json_builder_add_string_value (builder, "2.0");
    json_builder_end_object (builder);
    node = json_builder_get_root (builder);

    mcp_transport_send_message_async (MCP_TRANSPORT (transport), node,
                                      NULL, NULL, NULL);
    g_object_unref (transport);

    for (i = 0; i < 200; i++)
    {
        g_main_context_iteration (NULL, FALSE);
        g_usleep (500);
    }
}

static void
test_teardown_with_a_write_in_flight (void)
{
    if (g_test_subprocess ())
    {
        teardown_write_child ();
        return;
    }

    g_test_trap_subprocess (NULL, 10 * G_USEC_PER_SEC, 0);
    g_test_trap_assert_passed ();
}

/* ========================================================================== */
/* Main                                                                       */
/* ========================================================================== */

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

#define STDIO_TEST(path, func) \
    g_test_add (path, StdioFixture, NULL, stdio_fixture_set_up, func, \
                stdio_fixture_tear_down)

    STDIO_TEST ("/mcp/stdio/reads-a-line", test_reads_a_line);
    STDIO_TEST ("/mcp/stdio/reads-a-line-split-across-reads",
                test_reads_a_line_split_across_reads);
    STDIO_TEST ("/mcp/stdio/reads-two-lines-from-one-write",
                test_reads_two_lines_from_one_write);
    STDIO_TEST ("/mcp/stdio/writes-a-line", test_writes_a_line);
    STDIO_TEST ("/mcp/stdio/unterminated-line-is-refused",
                test_unterminated_line_is_refused);
    STDIO_TEST ("/mcp/stdio/line-at-the-limit-is-accepted",
                test_line_at_the_limit_is_accepted);

#undef STDIO_TEST

    g_test_add_func ("/mcp/stdio/teardown-with-reads-in-flight",
                     test_teardown_with_reads_in_flight);
    g_test_add_func ("/mcp/stdio/teardown-with-a-write-in-flight",
                     test_teardown_with_a_write_in_flight);

    return g_test_run ();
}
