/*
 * mcp-stdio-transport.c - Stdio transport implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define MCP_COMPILATION
#include <mcp/mcp-stdio-transport.h>
#include <mcp/mcp-error.h>
#undef MCP_COMPILATION

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include <unistd.h>
#include <signal.h>

/**
 * SECTION:mcp-stdio-transport
 * @title: McpStdioTransport
 * @short_description: Stdio transport for MCP
 *
 * #McpStdioTransport is a transport implementation that uses stdio streams
 * for communication. Messages are framed using newline-delimited JSON (NDJSON).
 */

struct _McpStdioTransport
{
    GObject parent_instance;

    McpTransportState state;

    /* I/O streams */
    GInputStream     *input;
    GOutputStream    *output;
    GDataInputStream *data_input;

    /* Subprocess (if created with new_subprocess) */
    GSubprocess *subprocess;

    /* Read loop cancellable */
    GCancellable *read_cancellable;

    /* Whether we own the streams (subprocess case) */
    gboolean owns_streams;
};

static void mcp_stdio_transport_iface_init (McpTransportInterface *iface);

G_DEFINE_TYPE_WITH_CODE (McpStdioTransport, mcp_stdio_transport, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MCP_TYPE_TRANSPORT,
                                                mcp_stdio_transport_iface_init))

enum
{
    PROP_0,
    PROP_INPUT_STREAM,
    PROP_OUTPUT_STREAM,
    PROP_SUBPROCESS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

/*
 * Forward declarations for read loop.
 */
static void start_read_loop (McpStdioTransport *self);
static void read_line_cb    (GObject      *source,
                             GAsyncResult *result,
                             gpointer      user_data);

/*
 * Set state and emit signal.
 */
static void
set_state (McpStdioTransport *self,
           McpTransportState  new_state)
{
    McpTransportState old_state;

    old_state = self->state;
    if (old_state != new_state)
    {
        self->state = new_state;
        mcp_transport_emit_state_changed (MCP_TRANSPORT (self), old_state, new_state);
    }
}

static void
mcp_stdio_transport_dispose (GObject *object)
{
    McpStdioTransport *self = MCP_STDIO_TRANSPORT (object);

    /* Cancel any pending reads */
    if (self->read_cancellable != NULL)
    {
        g_cancellable_cancel (self->read_cancellable);
        g_clear_object (&self->read_cancellable);
    }

    g_clear_object (&self->data_input);
    g_clear_object (&self->input);
    g_clear_object (&self->output);

    if (self->subprocess != NULL)
    {
        /* Send SIGTERM if still running */
        if (g_subprocess_get_if_exited (self->subprocess) == FALSE &&
            g_subprocess_get_if_signaled (self->subprocess) == FALSE)
        {
            g_subprocess_send_signal (self->subprocess, SIGTERM);
        }
        g_clear_object (&self->subprocess);
    }

    G_OBJECT_CLASS (mcp_stdio_transport_parent_class)->dispose (object);
}

static void
mcp_stdio_transport_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
    McpStdioTransport *self = MCP_STDIO_TRANSPORT (object);

    switch (prop_id)
    {
        case PROP_INPUT_STREAM:
            g_value_set_object (value, self->input);
            break;
        case PROP_OUTPUT_STREAM:
            g_value_set_object (value, self->output);
            break;
        case PROP_SUBPROCESS:
            g_value_set_object (value, self->subprocess);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_stdio_transport_class_init (McpStdioTransportClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = mcp_stdio_transport_dispose;
    object_class->get_property = mcp_stdio_transport_get_property;

    /**
     * McpStdioTransport:input-stream:
     *
     * The input stream for reading messages.
     */
    properties[PROP_INPUT_STREAM] =
        g_param_spec_object ("input-stream",
                             "Input Stream",
                             "The input stream for reading messages",
                             G_TYPE_INPUT_STREAM,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    /**
     * McpStdioTransport:output-stream:
     *
     * The output stream for writing messages.
     */
    properties[PROP_OUTPUT_STREAM] =
        g_param_spec_object ("output-stream",
                             "Output Stream",
                             "The output stream for writing messages",
                             G_TYPE_OUTPUT_STREAM,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    /**
     * McpStdioTransport:subprocess:
     *
     * The subprocess if created with mcp_stdio_transport_new_subprocess().
     */
    properties[PROP_SUBPROCESS] =
        g_param_spec_object ("subprocess",
                             "Subprocess",
                             "The subprocess if any",
                             G_TYPE_SUBPROCESS,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
mcp_stdio_transport_init (McpStdioTransport *self)
{
    self->state = MCP_TRANSPORT_STATE_DISCONNECTED;
}

/*
 * McpTransport interface implementation.
 */

static McpTransportState
stdio_transport_get_state (McpTransport *transport)
{
    McpStdioTransport *self = MCP_STDIO_TRANSPORT (transport);
    return self->state;
}

static void
stdio_transport_connect_async (McpTransport        *transport,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
    McpStdioTransport *self = MCP_STDIO_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, stdio_transport_connect_async);

    if (self->state != MCP_TRANSPORT_STATE_DISCONNECTED)
    {
        g_task_return_new_error (task,
                                 MCP_ERROR,
                                 MCP_ERROR_INTERNAL_ERROR,
                                 "Transport is not in disconnected state");
        return;
    }

    if (self->input == NULL || self->output == NULL)
    {
        g_task_return_new_error (task,
                                 MCP_ERROR,
                                 MCP_ERROR_INTERNAL_ERROR,
                                 "Input or output stream not set");
        return;
    }

    set_state (self, MCP_TRANSPORT_STATE_CONNECTING);

    /* Create data input stream wrapper for line reading */
    self->data_input = g_data_input_stream_new (self->input);
    g_data_input_stream_set_newline_type (self->data_input,
                                          G_DATA_STREAM_NEWLINE_TYPE_LF);

    /* Create cancellable for read loop */
    self->read_cancellable = g_cancellable_new ();

    /* Start the read loop */
    start_read_loop (self);

    set_state (self, MCP_TRANSPORT_STATE_CONNECTED);
    g_task_return_boolean (task, TRUE);
}

static gboolean
stdio_transport_connect_finish (McpTransport  *transport,
                                GAsyncResult  *result,
                                GError       **error)
{
    g_return_val_if_fail (g_task_is_valid (result, transport), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
stdio_transport_disconnect_async (McpTransport        *transport,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
    McpStdioTransport *self = MCP_STDIO_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, stdio_transport_disconnect_async);

    if (self->state == MCP_TRANSPORT_STATE_DISCONNECTED)
    {
        g_task_return_boolean (task, TRUE);
        return;
    }

    set_state (self, MCP_TRANSPORT_STATE_DISCONNECTING);

    /* Cancel read loop */
    if (self->read_cancellable != NULL)
    {
        g_cancellable_cancel (self->read_cancellable);
        g_clear_object (&self->read_cancellable);
    }

    /* Close streams */
    if (self->output != NULL)
    {
        g_output_stream_close (self->output, NULL, NULL);
    }
    if (self->input != NULL)
    {
        g_input_stream_close (self->input, NULL, NULL);
    }

    /* Kill subprocess if any */
    if (self->subprocess != NULL)
    {
        if (g_subprocess_get_if_exited (self->subprocess) == FALSE &&
            g_subprocess_get_if_signaled (self->subprocess) == FALSE)
        {
            g_subprocess_send_signal (self->subprocess, SIGTERM);
        }
    }

    set_state (self, MCP_TRANSPORT_STATE_DISCONNECTED);
    g_task_return_boolean (task, TRUE);
}

static gboolean
stdio_transport_disconnect_finish (McpTransport  *transport,
                                   GAsyncResult  *result,
                                   GError       **error)
{
    g_return_val_if_fail (g_task_is_valid (result, transport), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

/*
 * Data structure for async write operation.
 */
typedef struct
{
    gchar *line;
    gsize  len;
} WriteData;

static void
write_data_free (gpointer data)
{
    WriteData *wd = data;
    g_free (wd->line);
    g_free (wd);
}

static void
write_message_cb (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    GOutputStream *output = G_OUTPUT_STREAM (source);
    g_autoptr(GTask) task = G_TASK (user_data);
    GError *error = NULL;
    gsize bytes_written;

    if (!g_output_stream_write_all_finish (output, result, &bytes_written, &error))
    {
        g_task_return_error (task, error);
        return;
    }

    g_task_return_boolean (task, TRUE);
}

static void
stdio_transport_send_message_async (McpTransport        *transport,
                                    JsonNode            *message,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
    McpStdioTransport *self = MCP_STDIO_TRANSPORT (transport);
    GTask *task;
    g_autoptr(JsonGenerator) generator = NULL;
    g_autofree gchar *json_str = NULL;
    WriteData *wd;

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, stdio_transport_send_message_async);

    if (self->state != MCP_TRANSPORT_STATE_CONNECTED)
    {
        g_task_return_new_error (task,
                                 MCP_ERROR,
                                 MCP_ERROR_CONNECTION_CLOSED,
                                 "Transport is not connected");
        g_object_unref (task);
        return;
    }

    /* Serialize message to JSON */
    generator = json_generator_new ();
    json_generator_set_root (generator, message);
    json_str = json_generator_to_data (generator, NULL);

    /* Prepare write data with newline for NDJSON framing */
    wd = g_new0 (WriteData, 1);
    wd->line = g_strdup_printf ("%s\n", json_str);
    wd->len = strlen (wd->line);

    /* Attach data to task so it stays alive during async operation */
    g_task_set_task_data (task, wd, write_data_free);

    /* Write asynchronously */
    g_output_stream_write_all_async (self->output,
                                     wd->line,
                                     wd->len,
                                     G_PRIORITY_DEFAULT,
                                     cancellable,
                                     write_message_cb,
                                     task);
}

static gboolean
stdio_transport_send_message_finish (McpTransport  *transport,
                                     GAsyncResult  *result,
                                     GError       **error)
{
    g_return_val_if_fail (g_task_is_valid (result, transport), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_stdio_transport_iface_init (McpTransportInterface *iface)
{
    iface->get_state = stdio_transport_get_state;
    iface->connect_async = stdio_transport_connect_async;
    iface->connect_finish = stdio_transport_connect_finish;
    iface->disconnect_async = stdio_transport_disconnect_async;
    iface->disconnect_finish = stdio_transport_disconnect_finish;
    iface->send_message_async = stdio_transport_send_message_async;
    iface->send_message_finish = stdio_transport_send_message_finish;
}

/*
 * Read loop for receiving messages.
 */

static void
read_line_cb (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
    GDataInputStream *data_input = G_DATA_INPUT_STREAM (source);
    McpStdioTransport *self = MCP_STDIO_TRANSPORT (user_data);
    g_autofree gchar *line = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(JsonParser) parser = NULL;
    JsonNode *root;
    gsize length;

    line = g_data_input_stream_read_line_finish (data_input, result, &length, &error);

    /* Check for cancellation or EOF */
    if (line == NULL)
    {
        if (error != NULL)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            {
                mcp_transport_emit_error (MCP_TRANSPORT (self), error);
                set_state (self, MCP_TRANSPORT_STATE_ERROR);
            }
        }
        else
        {
            /* EOF - remote end closed connection */
            g_autoptr(GError) eof_error = NULL;
            eof_error = g_error_new (MCP_ERROR,
                                     MCP_ERROR_CONNECTION_CLOSED,
                                     "Remote end closed connection");
            mcp_transport_emit_error (MCP_TRANSPORT (self), eof_error);
            set_state (self, MCP_TRANSPORT_STATE_DISCONNECTED);
        }
        return;
    }

    /* Skip empty lines */
    if (length == 0)
    {
        start_read_loop (self);
        return;
    }

    /* Parse JSON */
    parser = json_parser_new ();
    if (!json_parser_load_from_data (parser, line, length, &error))
    {
        g_autoptr(GError) parse_error = NULL;
        parse_error = g_error_new (MCP_ERROR,
                                   MCP_ERROR_PARSE_ERROR,
                                   "Failed to parse JSON: %s",
                                   error->message);
        mcp_transport_emit_error (MCP_TRANSPORT (self), parse_error);
        /* Continue reading despite parse error */
        start_read_loop (self);
        return;
    }

    root = json_parser_get_root (parser);
    if (root == NULL)
    {
        g_autoptr(GError) parse_error = NULL;
        parse_error = g_error_new (MCP_ERROR,
                                   MCP_ERROR_PARSE_ERROR,
                                   "Empty JSON root");
        mcp_transport_emit_error (MCP_TRANSPORT (self), parse_error);
        start_read_loop (self);
        return;
    }

    /* Emit message-received signal */
    mcp_transport_emit_message_received (MCP_TRANSPORT (self), root);

    /* Continue read loop */
    start_read_loop (self);
}

static void
start_read_loop (McpStdioTransport *self)
{
    if (self->state != MCP_TRANSPORT_STATE_CONNECTED)
    {
        return;
    }

    if (self->read_cancellable == NULL ||
        g_cancellable_is_cancelled (self->read_cancellable))
    {
        return;
    }

    g_data_input_stream_read_line_async (self->data_input,
                                         G_PRIORITY_DEFAULT,
                                         self->read_cancellable,
                                         read_line_cb,
                                         self);
}

/*
 * Public constructors.
 */

/**
 * mcp_stdio_transport_new:
 *
 * Creates a new stdio transport that uses the process's stdin and stdout.
 *
 * Returns: (transfer full): a new #McpStdioTransport
 */
McpStdioTransport *
mcp_stdio_transport_new (void)
{
    McpStdioTransport *self;

    self = g_object_new (MCP_TYPE_STDIO_TRANSPORT, NULL);

    /* Use stdin and stdout */
    self->input = g_unix_input_stream_new (STDIN_FILENO, FALSE);
    self->output = g_unix_output_stream_new (STDOUT_FILENO, FALSE);
    self->owns_streams = FALSE;

    return self;
}

/**
 * mcp_stdio_transport_new_with_streams:
 * @input: (transfer none): the input stream to read from
 * @output: (transfer none): the output stream to write to
 *
 * Creates a new stdio transport with custom streams.
 *
 * Returns: (transfer full): a new #McpStdioTransport
 */
McpStdioTransport *
mcp_stdio_transport_new_with_streams (GInputStream  *input,
                                      GOutputStream *output)
{
    McpStdioTransport *self;

    g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);
    g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), NULL);

    self = g_object_new (MCP_TYPE_STDIO_TRANSPORT, NULL);

    self->input = g_object_ref (input);
    self->output = g_object_ref (output);
    self->owns_streams = FALSE;

    return self;
}

/**
 * mcp_stdio_transport_new_subprocess:
 * @command: (array zero-terminated=1): the command to execute
 * @error: (nullable): return location for a #GError
 *
 * Creates a new stdio transport that spawns a subprocess.
 *
 * Returns: (transfer full) (nullable): a new #McpStdioTransport, or %NULL on error
 */
McpStdioTransport *
mcp_stdio_transport_new_subprocess (const gchar * const *command,
                                    GError             **error)
{
    McpStdioTransport *self;
    g_autoptr(GSubprocessLauncher) launcher = NULL;
    g_autoptr(GSubprocess) subprocess = NULL;

    g_return_val_if_fail (command != NULL && command[0] != NULL, NULL);

    launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE);

    subprocess = g_subprocess_launcher_spawnv (launcher, command, error);
    if (subprocess == NULL)
    {
        return NULL;
    }

    self = g_object_new (MCP_TYPE_STDIO_TRANSPORT, NULL);

    self->subprocess = g_steal_pointer (&subprocess);
    self->input = g_object_ref (g_subprocess_get_stdout_pipe (self->subprocess));
    self->output = g_object_ref (g_subprocess_get_stdin_pipe (self->subprocess));
    self->owns_streams = TRUE;

    return self;
}

/**
 * mcp_stdio_transport_new_subprocess_simple:
 * @command_line: the command line to execute (parsed by shell rules)
 * @error: (nullable): return location for a #GError
 *
 * Creates a new stdio transport that spawns a subprocess using shell parsing.
 *
 * Returns: (transfer full) (nullable): a new #McpStdioTransport, or %NULL on error
 */
McpStdioTransport *
mcp_stdio_transport_new_subprocess_simple (const gchar  *command_line,
                                           GError      **error)
{
    g_auto(GStrv) argv = NULL;
    gint argc;

    g_return_val_if_fail (command_line != NULL, NULL);

    if (!g_shell_parse_argv (command_line, &argc, &argv, error))
    {
        return NULL;
    }

    return mcp_stdio_transport_new_subprocess ((const gchar * const *)argv, error);
}

/**
 * mcp_stdio_transport_get_subprocess:
 * @self: an #McpStdioTransport
 *
 * Gets the subprocess if this transport was created with
 * mcp_stdio_transport_new_subprocess().
 *
 * Returns: (transfer none) (nullable): the #GSubprocess, or %NULL
 */
GSubprocess *
mcp_stdio_transport_get_subprocess (McpStdioTransport *self)
{
    g_return_val_if_fail (MCP_IS_STDIO_TRANSPORT (self), NULL);

    return self->subprocess;
}

/**
 * mcp_stdio_transport_get_input_stream:
 * @self: an #McpStdioTransport
 *
 * Gets the input stream.
 *
 * Returns: (transfer none): the #GInputStream
 */
GInputStream *
mcp_stdio_transport_get_input_stream (McpStdioTransport *self)
{
    g_return_val_if_fail (MCP_IS_STDIO_TRANSPORT (self), NULL);

    return self->input;
}

/**
 * mcp_stdio_transport_get_output_stream:
 * @self: an #McpStdioTransport
 *
 * Gets the output stream.
 *
 * Returns: (transfer none): the #GOutputStream
 */
GOutputStream *
mcp_stdio_transport_get_output_stream (McpStdioTransport *self)
{
    g_return_val_if_fail (MCP_IS_STDIO_TRANSPORT (self), NULL);

    return self->output;
}
