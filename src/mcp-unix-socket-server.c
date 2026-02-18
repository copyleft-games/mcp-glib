/*
 * mcp-unix-socket-server.c - Unix domain socket MCP server for mcp-glib
 *
 * Copyright (C) 2026 Zach Podbielniak
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Manages a GSocketService on a Unix domain socket. Each incoming
 * client connection gets its own McpServer + McpStdioTransport pair.
 * The consumer registers tools/resources/prompts by connecting to
 * the "session-created" signal before calling start().
 */

#include "mcp-unix-socket-server.h"
#include "mcp-server.h"
#include "mcp-stdio-transport.h"
#include "mcp-transport.h"

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <unistd.h>

/* ===== Internal session structure ===== */

/*
 * McpUnixSocketSession:
 *
 * Tracks a single connected client. Each session owns one McpServer
 * and one McpStdioTransport wrapping the socket connection's streams.
 * The owner back-reference is unowned (no ref cycle).
 */
typedef struct _McpUnixSocketSession McpUnixSocketSession;

struct _McpUnixSocketSession
{
	McpServer             *server;
	McpStdioTransport     *transport;
	GSocketConnection     *connection;
	McpUnixSocketServer   *owner;             /* unowned back-ref */
	gulong                 state_handler_id;
};

/* ===== GObject struct ===== */

struct _McpUnixSocketServer
{
	GObject parent_instance;

	/* Configuration (construct-time, immutable after start) */
	gchar *server_name;
	gchar *server_version;
	gchar *socket_path;
	gchar *instructions;

	/* Socket listener */
	GSocketService *socket_service;
	gboolean        running;

	/* Active sessions */
	GList *sessions;   /* GList of McpUnixSocketSession* */
};

G_DEFINE_TYPE (McpUnixSocketServer, mcp_unix_socket_server, G_TYPE_OBJECT)

/* ===== Signals ===== */

enum
{
	SIGNAL_SESSION_CREATED,
	SIGNAL_SESSION_CLOSED,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

/* ===== Properties ===== */

enum
{
	PROP_0,
	PROP_SERVER_NAME,
	PROP_SERVER_VERSION,
	PROP_SOCKET_PATH,
	PROP_INSTRUCTIONS,
	PROP_SESSION_COUNT,
	PROP_RUNNING,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

/* ===== Session lifecycle helpers ===== */

/*
 * session_free:
 *
 * Disconnects signal handlers, stops the server, and frees all
 * resources owned by a session. Safe to call with NULL.
 */
static void
session_free (McpUnixSocketSession *session)
{
	if (session == NULL)
		return;

	/* Disconnect transport signal handler before stopping */
	if (session->transport != NULL && session->state_handler_id > 0)
		g_signal_handler_disconnect (session->transport,
		                             session->state_handler_id);

	if (session->server != NULL)
	{
		mcp_server_stop (session->server);
		g_object_unref (session->server);
	}
	if (session->transport != NULL)
		g_object_unref (session->transport);
	if (session->connection != NULL)
		g_object_unref (session->connection);

	g_free (session);
}

/* Forward declaration for the incoming handler */
static void remove_session (McpUnixSocketServer  *self,
                             McpUnixSocketSession *session);

/*
 * on_session_transport_state_changed:
 *
 * Called when a session's transport changes state. If the transport
 * disconnects or errors, the session is removed and freed.
 */
static void
on_session_transport_state_changed (
	McpTransport      *transport,
	McpTransportState  old_state,
	McpTransportState  new_state,
	gpointer           user_data
){
	McpUnixSocketSession *session;

	(void)transport;
	(void)old_state;

	session = (McpUnixSocketSession *)user_data;

	if (new_state == MCP_TRANSPORT_STATE_DISCONNECTED ||
	    new_state == MCP_TRANSPORT_STATE_ERROR)
	{
		g_debug ("mcp-unix-socket-server: session transport %s",
		         new_state == MCP_TRANSPORT_STATE_ERROR
		             ? "error" : "disconnected");
		remove_session (session->owner, session);
	}
}

/*
 * remove_session:
 *
 * Removes a session from the server's session list, emits the
 * "session-closed" signal, and frees the session. Guards against
 * double-removal (e.g. from reentrant signal handlers during stop).
 */
static void
remove_session (
	McpUnixSocketServer  *self,
	McpUnixSocketSession *session
){
	/* Guard against double-removal */
	if (g_list_find (self->sessions, session) == NULL)
		return;

	/* Remove from list first to prevent reentrant issues */
	self->sessions = g_list_remove (self->sessions, session);

	/* Emit session-closed before freeing */
	g_signal_emit (self, signals[SIGNAL_SESSION_CLOSED], 0, session->server);

	g_object_notify_by_pspec (G_OBJECT (self),
	                          properties[PROP_SESSION_COUNT]);

	session_free (session);
}

/*
 * on_session_server_started:
 *
 * Async callback for mcp_server_start_async() on a per-connection
 * server. On failure, removes the session.
 */
static void
on_session_server_started (
	GObject      *source,
	GAsyncResult *result,
	gpointer      user_data
){
	McpUnixSocketSession *session;
	g_autoptr(GError) error = NULL;

	(void)source;

	session = (McpUnixSocketSession *)user_data;

	if (!mcp_server_start_finish (session->server, result, &error))
	{
		g_warning ("mcp-unix-socket-server: session start failed: %s",
		           error->message);
		remove_session (session->owner, session);
	}
	else
	{
		g_debug ("mcp-unix-socket-server: session started");
	}
}

/* ===== Socket incoming handler ===== */

/*
 * on_incoming:
 *
 * Called when a new client connects to the Unix domain socket.
 * Creates a new McpServer + McpStdioTransport pair, emits
 * "session-created" so the consumer can register tools, then
 * starts the server async.
 */
static gboolean
on_incoming (
	GSocketService    *service,
	GSocketConnection *connection,
	GObject           *source_object,
	gpointer           user_data
){
	McpUnixSocketServer  *self;
	McpUnixSocketSession *session;
	GInputStream         *input;
	GOutputStream        *output;

	(void)service;
	(void)source_object;

	self = MCP_UNIX_SOCKET_SERVER (user_data);

	session = g_new0 (McpUnixSocketSession, 1);
	session->owner      = self;     /* unowned back-ref */
	session->connection = g_object_ref (connection);

	/* Wrap socket streams in McpStdioTransport (NDJSON framing) */
	input  = g_io_stream_get_input_stream (G_IO_STREAM (connection));
	output = g_io_stream_get_output_stream (G_IO_STREAM (connection));

	session->transport = mcp_stdio_transport_new_with_streams (input, output);

	/* Create per-connection server */
	session->server = mcp_server_new (self->server_name, self->server_version);
	mcp_server_set_transport (session->server,
	                          MCP_TRANSPORT (session->transport));

	/* Apply instructions if set */
	if (self->instructions != NULL)
		mcp_server_set_instructions (session->server, self->instructions);

	/* Let consumer register tools/resources/prompts */
	g_signal_emit (self, signals[SIGNAL_SESSION_CREATED], 0, session->server);

	/* Track session */
	self->sessions = g_list_prepend (self->sessions, session);
	g_object_notify_by_pspec (G_OBJECT (self),
	                          properties[PROP_SESSION_COUNT]);

	/* Watch for disconnect */
	session->state_handler_id = g_signal_connect (
		MCP_TRANSPORT (session->transport), "state-changed",
		G_CALLBACK (on_session_transport_state_changed), session);

	/* Start the MCP handshake */
	mcp_server_start_async (session->server, NULL,
	                        on_session_server_started, session);

	g_debug ("mcp-unix-socket-server: accepted connection");
	return TRUE;
}

/* ===== Public API ===== */

/**
 * mcp_unix_socket_server_new:
 * @server_name: the name passed to each per-connection #McpServer
 * @server_version: the version passed to each per-connection #McpServer
 * @socket_path: the absolute path for the Unix domain socket
 *
 * Creates a new Unix socket MCP server.
 *
 * Returns: (transfer full): a new #McpUnixSocketServer
 */
McpUnixSocketServer *
mcp_unix_socket_server_new (
	const gchar *server_name,
	const gchar *server_version,
	const gchar *socket_path
){
	g_return_val_if_fail (server_name != NULL, NULL);
	g_return_val_if_fail (server_version != NULL, NULL);
	g_return_val_if_fail (socket_path != NULL, NULL);

	return g_object_new (MCP_TYPE_UNIX_SOCKET_SERVER,
	                     "server-name", server_name,
	                     "server-version", server_version,
	                     "socket-path", socket_path,
	                     NULL);
}

/**
 * mcp_unix_socket_server_start:
 * @self: an #McpUnixSocketServer
 * @error: (nullable): return location for a #GError
 *
 * Starts listening on the Unix domain socket.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
mcp_unix_socket_server_start (
	McpUnixSocketServer  *self,
	GError              **error
){
	g_autoptr(GSocketAddress) address = NULL;

	g_return_val_if_fail (MCP_IS_UNIX_SOCKET_SERVER (self), FALSE);

	if (self->running)
	{
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		                     "Server is already running");
		return FALSE;
	}

	/* Remove stale socket file from a previous run */
	unlink (self->socket_path);

	self->socket_service = g_socket_service_new ();

	address = (GSocketAddress *)g_unix_socket_address_new (self->socket_path);
	if (!g_socket_listener_add_address (
		    G_SOCKET_LISTENER (self->socket_service),
		    address,
		    G_SOCKET_TYPE_STREAM,
		    G_SOCKET_PROTOCOL_DEFAULT,
		    NULL,   /* source_object */
		    NULL,   /* effective_address */
		    error))
	{
		g_clear_object (&self->socket_service);
		return FALSE;
	}

	g_signal_connect (self->socket_service, "incoming",
	                  G_CALLBACK (on_incoming), self);

	g_socket_service_start (self->socket_service);
	self->running = TRUE;

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RUNNING]);

	g_debug ("mcp-unix-socket-server: listening on %s", self->socket_path);
	return TRUE;
}

/**
 * mcp_unix_socket_server_stop:
 * @self: an #McpUnixSocketServer
 *
 * Stops the server and cleans up all sessions.
 */
void
mcp_unix_socket_server_stop (McpUnixSocketServer *self)
{
	g_return_if_fail (MCP_IS_UNIX_SOCKET_SERVER (self));

	if (!self->running)
		return;

	/* Stop accepting new connections */
	if (self->socket_service != NULL)
	{
		g_socket_service_stop (self->socket_service);
		g_clear_object (&self->socket_service);
	}

	/* Tear down all sessions */
	while (self->sessions != NULL)
	{
		McpUnixSocketSession *session;

		session = (McpUnixSocketSession *)self->sessions->data;

		/* Emit session-closed */
		g_signal_emit (self, signals[SIGNAL_SESSION_CLOSED], 0,
		               session->server);

		/* Remove from list before freeing to avoid reentrant issues */
		self->sessions = g_list_delete_link (self->sessions,
		                                     self->sessions);
		session_free (session);
	}

	g_object_notify_by_pspec (G_OBJECT (self),
	                          properties[PROP_SESSION_COUNT]);

	/* Remove socket file */
	if (self->socket_path != NULL)
		unlink (self->socket_path);

	self->running = FALSE;
	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RUNNING]);

	g_debug ("mcp-unix-socket-server: stopped");
}

const gchar *
mcp_unix_socket_server_get_socket_path (McpUnixSocketServer *self)
{
	g_return_val_if_fail (MCP_IS_UNIX_SOCKET_SERVER (self), NULL);
	return self->socket_path;
}

const gchar *
mcp_unix_socket_server_get_server_name (McpUnixSocketServer *self)
{
	g_return_val_if_fail (MCP_IS_UNIX_SOCKET_SERVER (self), NULL);
	return self->server_name;
}

const gchar *
mcp_unix_socket_server_get_server_version (McpUnixSocketServer *self)
{
	g_return_val_if_fail (MCP_IS_UNIX_SOCKET_SERVER (self), NULL);
	return self->server_version;
}

guint
mcp_unix_socket_server_get_session_count (McpUnixSocketServer *self)
{
	g_return_val_if_fail (MCP_IS_UNIX_SOCKET_SERVER (self), 0);
	return g_list_length (self->sessions);
}

gboolean
mcp_unix_socket_server_is_running (McpUnixSocketServer *self)
{
	g_return_val_if_fail (MCP_IS_UNIX_SOCKET_SERVER (self), FALSE);
	return self->running;
}

void
mcp_unix_socket_server_set_instructions (
	McpUnixSocketServer *self,
	const gchar         *instructions
){
	g_return_if_fail (MCP_IS_UNIX_SOCKET_SERVER (self));

	g_free (self->instructions);
	self->instructions = g_strdup (instructions);
	g_object_notify_by_pspec (G_OBJECT (self),
	                          properties[PROP_INSTRUCTIONS]);
}

const gchar *
mcp_unix_socket_server_get_instructions (McpUnixSocketServer *self)
{
	g_return_val_if_fail (MCP_IS_UNIX_SOCKET_SERVER (self), NULL);
	return self->instructions;
}

/* ===== GObject vfuncs ===== */

static void
mcp_unix_socket_server_set_property (
	GObject      *object,
	guint         prop_id,
	const GValue *value,
	GParamSpec   *pspec
){
	McpUnixSocketServer *self;

	self = MCP_UNIX_SOCKET_SERVER (object);

	switch (prop_id)
	{
	case PROP_SERVER_NAME:
		g_free (self->server_name);
		self->server_name = g_value_dup_string (value);
		break;
	case PROP_SERVER_VERSION:
		g_free (self->server_version);
		self->server_version = g_value_dup_string (value);
		break;
	case PROP_SOCKET_PATH:
		g_free (self->socket_path);
		self->socket_path = g_value_dup_string (value);
		break;
	case PROP_INSTRUCTIONS:
		mcp_unix_socket_server_set_instructions (self,
			g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
mcp_unix_socket_server_get_property (
	GObject    *object,
	guint       prop_id,
	GValue     *value,
	GParamSpec *pspec
){
	McpUnixSocketServer *self;

	self = MCP_UNIX_SOCKET_SERVER (object);

	switch (prop_id)
	{
	case PROP_SERVER_NAME:
		g_value_set_string (value, self->server_name);
		break;
	case PROP_SERVER_VERSION:
		g_value_set_string (value, self->server_version);
		break;
	case PROP_SOCKET_PATH:
		g_value_set_string (value, self->socket_path);
		break;
	case PROP_INSTRUCTIONS:
		g_value_set_string (value, self->instructions);
		break;
	case PROP_SESSION_COUNT:
		g_value_set_uint (value,
			g_list_length (self->sessions));
		break;
	case PROP_RUNNING:
		g_value_set_boolean (value, self->running);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
mcp_unix_socket_server_dispose (GObject *object)
{
	McpUnixSocketServer *self;

	self = MCP_UNIX_SOCKET_SERVER (object);

	/* Clean up sessions and socket service */
	mcp_unix_socket_server_stop (self);

	G_OBJECT_CLASS (mcp_unix_socket_server_parent_class)->dispose (object);
}

static void
mcp_unix_socket_server_finalize (GObject *object)
{
	McpUnixSocketServer *self;

	self = MCP_UNIX_SOCKET_SERVER (object);

	g_free (self->server_name);
	g_free (self->server_version);
	g_free (self->socket_path);
	g_free (self->instructions);

	G_OBJECT_CLASS (mcp_unix_socket_server_parent_class)->finalize (object);
}

static void
mcp_unix_socket_server_class_init (McpUnixSocketServerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = mcp_unix_socket_server_set_property;
	object_class->get_property = mcp_unix_socket_server_get_property;
	object_class->dispose      = mcp_unix_socket_server_dispose;
	object_class->finalize     = mcp_unix_socket_server_finalize;

	/**
	 * McpUnixSocketServer:server-name:
	 *
	 * The name passed to each per-connection #McpServer.
	 */
	properties[PROP_SERVER_NAME] =
		g_param_spec_string ("server-name",
		                     "Server Name",
		                     "Name for per-connection McpServer instances",
		                     NULL,
		                     G_PARAM_READWRITE |
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS);

	/**
	 * McpUnixSocketServer:server-version:
	 *
	 * The version passed to each per-connection #McpServer.
	 */
	properties[PROP_SERVER_VERSION] =
		g_param_spec_string ("server-version",
		                     "Server Version",
		                     "Version for per-connection McpServer instances",
		                     NULL,
		                     G_PARAM_READWRITE |
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS);

	/**
	 * McpUnixSocketServer:socket-path:
	 *
	 * The absolute path for the Unix domain socket.
	 */
	properties[PROP_SOCKET_PATH] =
		g_param_spec_string ("socket-path",
		                     "Socket Path",
		                     "Absolute path for the Unix domain socket",
		                     NULL,
		                     G_PARAM_READWRITE |
		                     G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS);

	/**
	 * McpUnixSocketServer:instructions:
	 *
	 * Instructions applied to every new per-connection #McpServer
	 * via mcp_server_set_instructions(). Takes effect on the next
	 * connection; does not affect existing sessions.
	 */
	properties[PROP_INSTRUCTIONS] =
		g_param_spec_string ("instructions",
		                     "Instructions",
		                     "Instructions applied to new sessions",
		                     NULL,
		                     G_PARAM_READWRITE |
		                     G_PARAM_STATIC_STRINGS);

	/**
	 * McpUnixSocketServer:session-count:
	 *
	 * The number of active client sessions.
	 */
	properties[PROP_SESSION_COUNT] =
		g_param_spec_uint ("session-count",
		                   "Session Count",
		                   "Number of active client sessions",
		                   0, G_MAXUINT, 0,
		                   G_PARAM_READABLE |
		                   G_PARAM_STATIC_STRINGS);

	/**
	 * McpUnixSocketServer:running:
	 *
	 * Whether the server is currently listening for connections.
	 */
	properties[PROP_RUNNING] =
		g_param_spec_boolean ("running",
		                      "Running",
		                      "Whether the server is listening",
		                      FALSE,
		                      G_PARAM_READABLE |
		                      G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPERTIES,
	                                   properties);

	/**
	 * McpUnixSocketServer::session-created:
	 * @self: the #McpUnixSocketServer
	 * @server: (transfer none): the newly created #McpServer
	 *
	 * Emitted when a new client connects to the Unix socket.
	 * The server has its transport set and instructions applied,
	 * but has NOT been started yet.
	 *
	 * Connect to this signal to register tools, resources, and prompts
	 * on the new #McpServer before it begins the MCP handshake.
	 */
	signals[SIGNAL_SESSION_CREATED] =
		g_signal_new ("session-created",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,                        /* no class offset */
		              NULL, NULL,               /* accumulator */
		              NULL,                     /* C marshaller */
		              G_TYPE_NONE, 1,           /* return void, 1 param */
		              MCP_TYPE_SERVER);         /* param type */

	/**
	 * McpUnixSocketServer::session-closed:
	 * @self: the #McpUnixSocketServer
	 * @server: (transfer none): the #McpServer whose connection closed
	 *
	 * Emitted when a client connection closes (gracefully or on error).
	 * The server has already been stopped. Use this for cleanup.
	 */
	signals[SIGNAL_SESSION_CLOSED] =
		g_signal_new ("session-closed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE, 1,
		              MCP_TYPE_SERVER);
}

static void
mcp_unix_socket_server_init (McpUnixSocketServer *self)
{
	self->server_name    = NULL;
	self->server_version = NULL;
	self->socket_path    = NULL;
	self->instructions   = NULL;
	self->socket_service = NULL;
	self->running        = FALSE;
	self->sessions       = NULL;
}
