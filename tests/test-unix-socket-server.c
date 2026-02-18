/*
 * test-unix-socket-server.c - Tests for McpUnixSocketServer
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <unistd.h>
#include "mcp.h"

/* ===== Test helpers ===== */

/*
 * make_test_socket_path:
 *
 * Generates a unique socket path in the runtime dir for each test
 * to avoid collisions. Caller owns the returned string.
 */
static gchar *
make_test_socket_path (const gchar *test_name)
{
	return g_strdup_printf ("%s/mcp-test-%s-%d.sock",
	                        g_get_user_runtime_dir (),
	                        test_name,
	                        (gint)getpid ());
}

/*
 * cleanup_socket:
 *
 * Removes a socket file if it exists.
 */
static void
cleanup_socket (const gchar *path)
{
	if (path != NULL)
		unlink (path);
}

/* ============================================================================
 * Constructor Tests
 * ========================================================================== */

static void
test_unix_socket_server_new (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;

	path = make_test_socket_path ("new");
	server = mcp_unix_socket_server_new ("test-server", "1.0.0", path);

	g_assert_nonnull (server);
	g_assert_true (MCP_IS_UNIX_SOCKET_SERVER (server));
}

static void
test_unix_socket_server_new_verifies_type (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;

	path = make_test_socket_path ("type");
	server = mcp_unix_socket_server_new ("test-server", "1.0.0", path);

	g_assert_true (G_IS_OBJECT (server));
	g_assert_false (MCP_IS_TRANSPORT (server));
}

/* ============================================================================
 * Property Tests
 * ========================================================================== */

static void
test_unix_socket_server_server_name (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;

	path = make_test_socket_path ("name");
	server = mcp_unix_socket_server_new ("my-server", "2.0.0", path);

	g_assert_cmpstr (mcp_unix_socket_server_get_server_name (server),
	                 ==, "my-server");
}

static void
test_unix_socket_server_server_version (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;

	path = make_test_socket_path ("version");
	server = mcp_unix_socket_server_new ("test", "3.1.4", path);

	g_assert_cmpstr (mcp_unix_socket_server_get_server_version (server),
	                 ==, "3.1.4");
}

static void
test_unix_socket_server_socket_path (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;

	path = make_test_socket_path ("path");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_assert_cmpstr (mcp_unix_socket_server_get_socket_path (server),
	                 ==, path);
}

static void
test_unix_socket_server_instructions (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;

	path = make_test_socket_path ("instr");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	/* Default is NULL */
	g_assert_null (mcp_unix_socket_server_get_instructions (server));

	/* Set and verify */
	mcp_unix_socket_server_set_instructions (server, "Hello AI");
	g_assert_cmpstr (mcp_unix_socket_server_get_instructions (server),
	                 ==, "Hello AI");

	/* Clear */
	mcp_unix_socket_server_set_instructions (server, NULL);
	g_assert_null (mcp_unix_socket_server_get_instructions (server));
}

static void
test_unix_socket_server_session_count_initial (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;

	path = make_test_socket_path ("count");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_assert_cmpuint (mcp_unix_socket_server_get_session_count (server),
	                  ==, 0);
}

static void
test_unix_socket_server_running_initial (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;

	path = make_test_socket_path ("running");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_assert_false (mcp_unix_socket_server_is_running (server));
}

static void
test_unix_socket_server_gobject_properties (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;
	g_autofree gchar *name = NULL;
	g_autofree gchar *version = NULL;
	g_autofree gchar *sock_path = NULL;
	g_autofree gchar *instr = NULL;
	guint count = 99;
	gboolean running = TRUE;

	path = make_test_socket_path ("gobj");
	server = mcp_unix_socket_server_new ("test-srv", "1.2.3", path);
	mcp_unix_socket_server_set_instructions (server, "test instructions");

	g_object_get (G_OBJECT (server),
	              "server-name", &name,
	              "server-version", &version,
	              "socket-path", &sock_path,
	              "instructions", &instr,
	              "session-count", &count,
	              "running", &running,
	              NULL);

	g_assert_cmpstr (name, ==, "test-srv");
	g_assert_cmpstr (version, ==, "1.2.3");
	g_assert_cmpstr (sock_path, ==, path);
	g_assert_cmpstr (instr, ==, "test instructions");
	g_assert_cmpuint (count, ==, 0);
	g_assert_false (running);
}

/* ============================================================================
 * Start / Stop Tests
 * ========================================================================== */

static void
test_unix_socket_server_start_stop (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;
	g_autoptr(GError) error = NULL;

	path = make_test_socket_path ("startstop");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_assert_true (mcp_unix_socket_server_start (server, &error));
	g_assert_no_error (error);
	g_assert_true (mcp_unix_socket_server_is_running (server));

	mcp_unix_socket_server_stop (server);
	g_assert_false (mcp_unix_socket_server_is_running (server));
}

static void
test_unix_socket_server_start_creates_socket (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;
	g_autoptr(GError) error = NULL;

	path = make_test_socket_path ("creates");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_assert_false (g_file_test (path, G_FILE_TEST_EXISTS));

	g_assert_true (mcp_unix_socket_server_start (server, &error));
	g_assert_no_error (error);

	g_assert_true (g_file_test (path, G_FILE_TEST_EXISTS));

	mcp_unix_socket_server_stop (server);
}

static void
test_unix_socket_server_stop_removes_socket (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;
	g_autoptr(GError) error = NULL;

	path = make_test_socket_path ("removes");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_assert_true (mcp_unix_socket_server_start (server, &error));
	g_assert_no_error (error);
	g_assert_true (g_file_test (path, G_FILE_TEST_EXISTS));

	mcp_unix_socket_server_stop (server);
	g_assert_false (g_file_test (path, G_FILE_TEST_EXISTS));
}

static void
test_unix_socket_server_start_stale_socket (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;
	g_autoptr(GError) error = NULL;

	path = make_test_socket_path ("stale");

	/* Create a stale file at the socket path */
	g_file_set_contents (path, "stale", -1, NULL);
	g_assert_true (g_file_test (path, G_FILE_TEST_EXISTS));

	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	/* Should succeed by unlinking the stale file first */
	g_assert_true (mcp_unix_socket_server_start (server, &error));
	g_assert_no_error (error);

	mcp_unix_socket_server_stop (server);
}

static void
test_unix_socket_server_double_start (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;
	g_autoptr(GError) error1 = NULL;
	g_autoptr(GError) error2 = NULL;

	path = make_test_socket_path ("double");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_assert_true (mcp_unix_socket_server_start (server, &error1));
	g_assert_no_error (error1);

	/* Second start should fail */
	g_assert_false (mcp_unix_socket_server_start (server, &error2));
	g_assert_nonnull (error2);

	mcp_unix_socket_server_stop (server);
}

static void
test_unix_socket_server_stop_when_not_running (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;

	path = make_test_socket_path ("nop");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	/* Should be a no-op, not crash */
	mcp_unix_socket_server_stop (server);
	g_assert_false (mcp_unix_socket_server_is_running (server));
}

/* ============================================================================
 * Connection / Session Tests
 * ========================================================================== */

/* Context for async connection tests */
typedef struct
{
	GMainLoop           *loop;
	McpUnixSocketServer *server;
	gint                 created_count;
	gint                 closed_count;
	McpServer           *last_created_server;
} SessionTestCtx;

static void
on_session_created (
	McpUnixSocketServer *unix_server,
	McpServer           *mcp_server,
	gpointer             user_data
){
	SessionTestCtx *ctx;

	(void)unix_server;

	ctx = (SessionTestCtx *)user_data;
	ctx->created_count++;
	ctx->last_created_server = mcp_server;
}

static void
on_session_closed (
	McpUnixSocketServer *unix_server,
	McpServer           *mcp_server,
	gpointer             user_data
){
	SessionTestCtx *ctx;

	(void)unix_server;
	(void)mcp_server;

	ctx = (SessionTestCtx *)user_data;
	ctx->closed_count++;
}

/*
 * connect_client:
 *
 * Connects a GSocketClient to the Unix socket and returns the
 * connection. Caller owns the returned connection.
 */
static GSocketConnection *
connect_client (const gchar *path, GError **error)
{
	g_autoptr(GSocketClient) client = NULL;
	g_autoptr(GSocketAddress) address = NULL;
	GSocketConnection *conn;

	client  = g_socket_client_new ();
	address = (GSocketAddress *)g_unix_socket_address_new (path);

	conn = g_socket_client_connect (client,
	                                G_SOCKET_CONNECTABLE (address),
	                                NULL, error);
	return conn;
}

/*
 * spin_mainloop:
 *
 * Runs the main loop briefly to process pending events.
 */
static void
spin_mainloop (void)
{
	gint i;

	/* Process several iterations to let async callbacks fire */
	for (i = 0; i < 20; i++)
		g_main_context_iteration (NULL, FALSE);
}

static void
test_unix_socket_server_session_created_signal (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GSocketConnection) conn = NULL;
	SessionTestCtx ctx = { NULL, NULL, 0, 0, NULL };

	path = make_test_socket_path ("sig-created");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_signal_connect (server, "session-created",
	                  G_CALLBACK (on_session_created), &ctx);

	g_assert_true (mcp_unix_socket_server_start (server, &error));
	g_assert_no_error (error);

	/* Connect a client */
	conn = connect_client (path, &error);
	g_assert_no_error (error);
	g_assert_nonnull (conn);

	spin_mainloop ();

	g_assert_cmpint (ctx.created_count, ==, 1);
	g_assert_nonnull (ctx.last_created_server);
	g_assert_true (MCP_IS_SERVER (ctx.last_created_server));

	mcp_unix_socket_server_stop (server);
}

static void
test_unix_socket_server_session_count_after_connect (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GSocketConnection) conn = NULL;

	path = make_test_socket_path ("count-conn");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_assert_true (mcp_unix_socket_server_start (server, &error));
	g_assert_no_error (error);

	conn = connect_client (path, &error);
	g_assert_no_error (error);

	spin_mainloop ();

	g_assert_cmpuint (mcp_unix_socket_server_get_session_count (server),
	                  ==, 1);

	mcp_unix_socket_server_stop (server);
}

static void
test_unix_socket_server_multiple_sessions (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GSocketConnection) conn1 = NULL;
	g_autoptr(GSocketConnection) conn2 = NULL;
	SessionTestCtx ctx = { NULL, NULL, 0, 0, NULL };

	path = make_test_socket_path ("multi");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_signal_connect (server, "session-created",
	                  G_CALLBACK (on_session_created), &ctx);

	g_assert_true (mcp_unix_socket_server_start (server, &error));
	g_assert_no_error (error);

	conn1 = connect_client (path, &error);
	g_assert_no_error (error);
	spin_mainloop ();

	conn2 = connect_client (path, &error);
	g_assert_no_error (error);
	spin_mainloop ();

	g_assert_cmpint (ctx.created_count, ==, 2);
	g_assert_cmpuint (mcp_unix_socket_server_get_session_count (server),
	                  ==, 2);

	mcp_unix_socket_server_stop (server);
}

static void
test_unix_socket_server_instructions_applied (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GSocketConnection) conn = NULL;
	SessionTestCtx ctx = { NULL, NULL, 0, 0, NULL };

	path = make_test_socket_path ("instr-apply");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	mcp_unix_socket_server_set_instructions (server,
		"Test instructions for AI");

	g_signal_connect (server, "session-created",
	                  G_CALLBACK (on_session_created), &ctx);

	g_assert_true (mcp_unix_socket_server_start (server, &error));
	g_assert_no_error (error);

	conn = connect_client (path, &error);
	g_assert_no_error (error);
	spin_mainloop ();

	/* Verify instructions were applied to the per-connection server */
	g_assert_nonnull (ctx.last_created_server);
	g_assert_cmpstr (
		mcp_server_get_instructions (ctx.last_created_server),
		==, "Test instructions for AI");

	mcp_unix_socket_server_stop (server);
}

static void
test_unix_socket_server_stop_closes_all_sessions (void)
{
	g_autofree gchar *path = NULL;
	g_autoptr(McpUnixSocketServer) server = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GSocketConnection) conn1 = NULL;
	g_autoptr(GSocketConnection) conn2 = NULL;
	SessionTestCtx ctx = { NULL, NULL, 0, 0, NULL };

	path = make_test_socket_path ("stop-all");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_signal_connect (server, "session-created",
	                  G_CALLBACK (on_session_created), &ctx);
	g_signal_connect (server, "session-closed",
	                  G_CALLBACK (on_session_closed), &ctx);

	g_assert_true (mcp_unix_socket_server_start (server, &error));
	g_assert_no_error (error);

	conn1 = connect_client (path, &error);
	g_assert_no_error (error);
	spin_mainloop ();

	conn2 = connect_client (path, &error);
	g_assert_no_error (error);
	spin_mainloop ();

	g_assert_cmpint (ctx.created_count, ==, 2);
	g_assert_cmpint (ctx.closed_count, ==, 0);

	/* Stop should close all sessions */
	mcp_unix_socket_server_stop (server);

	g_assert_cmpint (ctx.closed_count, ==, 2);
	g_assert_cmpuint (mcp_unix_socket_server_get_session_count (server),
	                  ==, 0);
}

static void
test_unix_socket_server_dispose_stops (void)
{
	g_autofree gchar *path = NULL;
	McpUnixSocketServer *server;
	g_autoptr(GError) error = NULL;
	g_autoptr(GSocketConnection) conn = NULL;

	path = make_test_socket_path ("dispose");
	server = mcp_unix_socket_server_new ("test", "1.0.0", path);

	g_assert_true (mcp_unix_socket_server_start (server, &error));
	g_assert_no_error (error);

	conn = connect_client (path, &error);
	g_assert_no_error (error);
	spin_mainloop ();

	/* Unreffing should trigger dispose -> stop -> cleanup */
	g_object_unref (server);

	/* Socket file should be cleaned up */
	g_assert_false (g_file_test (path, G_FILE_TEST_EXISTS));
}

/* ============================================================================
 * Main
 * ========================================================================== */

gint
main (gint argc, gchar **argv)
{
	g_test_init (&argc, &argv, NULL);

	/* Constructor tests */
	g_test_add_func ("/mcp/unix-socket-server/new",
	                 test_unix_socket_server_new);
	g_test_add_func ("/mcp/unix-socket-server/new/verifies-type",
	                 test_unix_socket_server_new_verifies_type);

	/* Property tests */
	g_test_add_func ("/mcp/unix-socket-server/properties/server-name",
	                 test_unix_socket_server_server_name);
	g_test_add_func ("/mcp/unix-socket-server/properties/server-version",
	                 test_unix_socket_server_server_version);
	g_test_add_func ("/mcp/unix-socket-server/properties/socket-path",
	                 test_unix_socket_server_socket_path);
	g_test_add_func ("/mcp/unix-socket-server/properties/instructions",
	                 test_unix_socket_server_instructions);
	g_test_add_func ("/mcp/unix-socket-server/properties/session-count-initial",
	                 test_unix_socket_server_session_count_initial);
	g_test_add_func ("/mcp/unix-socket-server/properties/running-initial",
	                 test_unix_socket_server_running_initial);
	g_test_add_func ("/mcp/unix-socket-server/properties/gobject-get",
	                 test_unix_socket_server_gobject_properties);

	/* Start / Stop tests */
	g_test_add_func ("/mcp/unix-socket-server/start-stop",
	                 test_unix_socket_server_start_stop);
	g_test_add_func ("/mcp/unix-socket-server/start-creates-socket",
	                 test_unix_socket_server_start_creates_socket);
	g_test_add_func ("/mcp/unix-socket-server/stop-removes-socket",
	                 test_unix_socket_server_stop_removes_socket);
	g_test_add_func ("/mcp/unix-socket-server/start-stale-socket",
	                 test_unix_socket_server_start_stale_socket);
	g_test_add_func ("/mcp/unix-socket-server/double-start",
	                 test_unix_socket_server_double_start);
	g_test_add_func ("/mcp/unix-socket-server/stop-when-not-running",
	                 test_unix_socket_server_stop_when_not_running);

	/* Connection / Session tests */
	g_test_add_func ("/mcp/unix-socket-server/session/created-signal",
	                 test_unix_socket_server_session_created_signal);
	g_test_add_func ("/mcp/unix-socket-server/session/count-after-connect",
	                 test_unix_socket_server_session_count_after_connect);
	g_test_add_func ("/mcp/unix-socket-server/session/multiple",
	                 test_unix_socket_server_multiple_sessions);
	g_test_add_func ("/mcp/unix-socket-server/session/instructions-applied",
	                 test_unix_socket_server_instructions_applied);
	g_test_add_func ("/mcp/unix-socket-server/session/stop-closes-all",
	                 test_unix_socket_server_stop_closes_all_sessions);
	g_test_add_func ("/mcp/unix-socket-server/session/dispose-stops",
	                 test_unix_socket_server_dispose_stops);

	return g_test_run ();
}
