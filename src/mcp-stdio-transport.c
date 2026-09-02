/*
 * mcp-stdio-transport.c - Stdio transport implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp-stdio-transport.h"
#include "mcp-error.h"
#undef MCP_COMPILATION

/*
 * Platform-specific includes for stdio streams.
 * Windows uses Win32 handles, Unix uses file descriptors.
 */
#ifdef _WIN32
#include <gio/gwin32inputstream.h>
#include <gio/gwin32outputstream.h>
#include <windows.h>
#else
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <unistd.h>
#include <signal.h>
#endif

/**
 * SECTION:mcp-stdio-transport
 * @title: McpStdioTransport
 * @short_description: Stdio transport for MCP
 *
 * #McpStdioTransport is a transport implementation that uses stdio streams
 * for communication. Messages are framed using newline-delimited JSON (NDJSON).
 */

/*
 * How much is asked of the input stream per read.  Framing is done here
 * rather than by GDataInputStream, so this is a read size and not a limit
 * on anything.
 */
#define MCP_STDIO_READ_CHUNK_BYTES (64 * 1024)

struct _McpStdioTransport
{
    GObject parent_instance;

    McpTransportState state;

    /* I/O streams */
    GInputStream     *input;
    GOutputStream    *output;

    /* Bytes read but not yet terminated by a newline.  This is what the
     * line cap is applied to, before the next read rather than after it. */
    GString          *inbound;
    gsize             max_line_bytes;

    /* Subprocess (if created with new_subprocess) */
    GSubprocess *subprocess;

    /* Read loop cancellable */
    GCancellable *read_cancellable;

    /* Whether we own the streams (subprocess case) */
    gboolean owns_streams;

    /* Write queue to handle sequential async writes */
    GQueue *write_queue;
    gboolean write_in_progress;
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
    PROP_MAX_LINE_BYTES,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

/*
 * Forward declarations for read loop.
 */
static void start_read_loop (McpStdioTransport *self);
static void read_chunk_cb   (GObject      *source,
                             GAsyncResult *result,
                             gpointer      user_data);
static gboolean start_read_loop_idle (gpointer user_data);

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

/*
 * WriteQueueEntry - entry in the write queue
 */
typedef struct
{
    GTask *task;
    gchar *line;
    gsize  len;
} WriteQueueEntry;

static void
write_queue_entry_free (WriteQueueEntry *entry)
{
    if (entry != NULL)
    {
        g_clear_object (&entry->task);
        g_free (entry->line);
        g_free (entry);
    }
}

static void process_write_queue (McpStdioTransport *self);

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

    /* Clear write queue */
    if (self->write_queue != NULL)
    {
        WriteQueueEntry *entry;
        while ((entry = g_queue_pop_head (self->write_queue)) != NULL)
        {
            if (entry->task != NULL)
            {
                g_task_return_new_error (entry->task,
                                         MCP_ERROR,
                                         MCP_ERROR_CONNECTION_CLOSED,
                                         "Transport disposed");
            }
            write_queue_entry_free (entry);
        }
        g_queue_free (self->write_queue);
        self->write_queue = NULL;
    }

    if (self->inbound != NULL)
    {
        g_string_free (self->inbound, TRUE);
        self->inbound = NULL;
    }

    g_clear_object (&self->input);
    g_clear_object (&self->output);

    if (self->subprocess != NULL)
    {
        /* Unconditionally terminate the subprocess on dispose.
         * g_subprocess_get_if_exited() requires pid==0 (already reaped),
         * so calling it on a running process triggers a GLib assertion.
         * Just send the signal -- it is a no-op if the process has already
         * exited and safe to call at any time. */
#ifdef _WIN32
        g_subprocess_force_exit (self->subprocess);
#else
        g_subprocess_send_signal (self->subprocess, SIGTERM);
#endif
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
        case PROP_MAX_LINE_BYTES:
            g_value_set_uint64 (value, (guint64) self->max_line_bytes);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_stdio_transport_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
    McpStdioTransport *self = MCP_STDIO_TRANSPORT (object);

    switch (prop_id)
    {
        case PROP_MAX_LINE_BYTES:
            mcp_stdio_transport_set_max_line_bytes (self,
                                                    (gsize) g_value_get_uint64 (value));
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
    object_class->set_property = mcp_stdio_transport_set_property;

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

    /**
     * McpStdioTransport:max-line-bytes:
     *
     * The cap on a single newline-delimited message, or 0 for none.
     */
    properties[PROP_MAX_LINE_BYTES] =
        g_param_spec_uint64 ("max-line-bytes",
                             "Max Line Bytes",
                             "Cap on a single newline-delimited message",
                             0, G_MAXUINT64,
                             MCP_STDIO_TRANSPORT_DEFAULT_MAX_LINE_BYTES,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
mcp_stdio_transport_init (McpStdioTransport *self)
{
    self->state = MCP_TRANSPORT_STATE_DISCONNECTED;
    self->write_queue = g_queue_new ();
    self->write_in_progress = FALSE;
    self->inbound = g_string_new (NULL);
    self->max_line_bytes = MCP_STDIO_TRANSPORT_DEFAULT_MAX_LINE_BYTES;
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

    /* Anything left over from a previous connection is not part of this
     * one's first line. */
    g_string_truncate (self->inbound, 0);

    /* Create cancellable for read loop */
    self->read_cancellable = g_cancellable_new ();

    /* Set state to connected */
    set_state (self, MCP_TRANSPORT_STATE_CONNECTED);

    g_task_return_boolean (task, TRUE);

    /* Schedule the read loop to start in an idle callback so the
     * connect callback can complete first.
     * Use the thread-default GMainContext so this works when the
     * transport runs on a dedicated thread with its own context
     * (e.g. a Wayland compositor that uses wl_event_loop instead
     * of the default GMainContext). */
    {
        GSource *idle_source;

        idle_source = g_idle_source_new ();
        g_source_set_callback (idle_source, start_read_loop_idle,
                               g_object_ref (self), g_object_unref);
        g_source_attach (idle_source,
                         g_main_context_get_thread_default ());
        g_source_unref (idle_source);
    }
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

    /* Terminate subprocess if any.  See dispose for why we skip the
     * g_subprocess_get_if_exited() check. */
    if (self->subprocess != NULL)
    {
#ifdef _WIN32
        g_subprocess_force_exit (self->subprocess);
#else
        g_subprocess_send_signal (self->subprocess, SIGTERM);
#endif
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
 * WriteOp:
 *
 * One write in flight.  Like ReadOp it owns what GIO is still using --
 * the stream and the bytes -- and holds a weak reference to the
 * transport.  The bytes are *stolen* from the queue entry rather than
 * left in it: dispose frees the queue, and it used to free the buffer
 * out from under a write that GIO was still reading from.
 */
typedef struct
{
    GWeakRef       transport;
    GOutputStream *stream;   /* owned */
    gchar         *data;     /* owned: taken from the queue entry */
    gsize          len;
} WriteOp;

static void
write_op_free (WriteOp *op)
{
    g_weak_ref_clear (&op->transport);
    g_clear_object (&op->stream);
    g_free (op->data);
    g_free (op);
}

/*
 * write_message_cb:
 *
 * Callback for async write completion. Completes the current task
 * and processes the next item in the write queue.
 */
static void
write_message_cb (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    WriteOp *op = user_data;
    g_autoptr(McpStdioTransport) self = g_weak_ref_get (&op->transport);
    WriteQueueEntry *entry;
    GError *error = NULL;
    gsize bytes_written;
    gboolean ok;

    ok = g_output_stream_write_all_finish (G_OUTPUT_STREAM (source), result,
                                           &bytes_written, &error);
    write_op_free (op);

    /* The transport went away while the write was in flight; dispose
     * already answered every queued task. */
    if (self == NULL)
    {
        g_clear_error (&error);
        return;
    }

    /* Pop the entry we just wrote */
    entry = g_queue_pop_head (self->write_queue);
    g_return_if_fail (entry != NULL);

    if (!ok)
    {
        if (entry->task != NULL)
        {
            g_task_return_error (entry->task, error);
        }
        else
        {
            g_error_free (error);
        }
    }
    else
    {
        if (entry->task != NULL)
        {
            g_task_return_boolean (entry->task, TRUE);
        }
    }

    write_queue_entry_free (entry);

    /* Mark write as complete and process next in queue */
    self->write_in_progress = FALSE;
    process_write_queue (self);
}

/*
 * process_write_queue:
 *
 * Processes the next item in the write queue if no write is in progress.
 */
static void
process_write_queue (McpStdioTransport *self)
{
    WriteQueueEntry *entry;
    WriteOp *op;

    if (self->write_in_progress)
    {
        return;
    }

    if (self->state != MCP_TRANSPORT_STATE_CONNECTED)
    {
        return;
    }

    entry = g_queue_peek_head (self->write_queue);
    if (entry == NULL)
    {
        return;
    }

    op = g_new0 (WriteOp, 1);
    g_weak_ref_init (&op->transport, self);
    op->stream = g_object_ref (self->output);
    op->data = g_steal_pointer (&entry->line);
    op->len = entry->len;

    self->write_in_progress = TRUE;
    g_output_stream_write_all_async (op->stream,
                                     op->data,
                                     op->len,
                                     G_PRIORITY_DEFAULT,
                                     NULL,
                                     write_message_cb,
                                     op);
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
    WriteQueueEntry *entry;

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

    /* Create queue entry */
    entry = g_new0 (WriteQueueEntry, 1);
    entry->task = task;  /* Takes ownership */
    entry->line = g_strdup_printf ("%s\n", json_str);
    entry->len = strlen (entry->line);

    /* Add to queue and process */
    g_queue_push_tail (self->write_queue, entry);
    process_write_queue (self);
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
 *
 * Framing is done here rather than by GDataInputStream because
 * g_data_input_stream_read_line_async() grows its own buffer -- doubling
 * it -- until it finds a newline, whatever
 * g_buffered_input_stream_set_buffer_size() was set to.  Measured: a
 * 4 KiB buffer size and 8 MiB of newline-free input leaves an 8 MiB
 * buffer.  A cap applied to the returned line is a check on memory that
 * has already been committed, so the cap has to be on what accumulates.
 */

/*
 * ReadOp:
 *
 * One read in flight.  It owns everything GIO will touch after the
 * callback is scheduled -- the stream and the buffer being filled -- and
 * holds only a *weak* reference to the transport.
 *
 * A strong one would be a cycle with the thing that ends the read:
 * unreffing the transport is what runs dispose, and dispose is what
 * cancels the read loop and SIGTERMs the subprocess.  Parking a strong
 * reference in the callback would turn the use-after-free into a
 * transport that never goes away, holding a child nobody kills.
 */
typedef struct
{
    GWeakRef      transport;
    GInputStream *stream;   /* owned: GIO reads from it after finalize */
    guint8        chunk[MCP_STDIO_READ_CHUNK_BYTES];
} ReadOp;

static ReadOp *
read_op_new (McpStdioTransport *self)
{
    ReadOp *op;

    op = g_new0 (ReadOp, 1);
    g_weak_ref_init (&op->transport, self);
    op->stream = g_object_ref (self->input);

    return op;
}

static void
read_op_free (ReadOp *op)
{
    g_weak_ref_clear (&op->transport);
    g_clear_object (&op->stream);
    g_free (op);
}

/*
 * dispatch_line:
 *
 * Parses one complete frame and emits it.  A parse failure is reported
 * and the loop continues, as it always has: a peer that sends one bad
 * line has not necessarily lost the connection.
 */
static void
dispatch_line (McpStdioTransport *self,
               const gchar       *line,
               gsize              length)
{
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    parser = json_parser_new ();
    if (!json_parser_load_from_data (parser, line, (gssize) length, &error))
    {
        g_autoptr(GError) parse_error = NULL;

        parse_error = g_error_new (MCP_ERROR,
                                   MCP_ERROR_PARSE_ERROR,
                                   "Failed to parse JSON: %s",
                                   error->message);
        mcp_transport_emit_error (MCP_TRANSPORT (self), parse_error);
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
        return;
    }

    mcp_transport_emit_message_received (MCP_TRANSPORT (self), root);
}

/*
 * refuse_oversized_line:
 *
 * A peer that has sent more than the cap without a newline is not going
 * to become valid by being read from again, so the connection ends here
 * rather than after the memory has been spent.
 */
static void
refuse_oversized_line (McpStdioTransport *self)
{
    g_autoptr(GError) error = NULL;

    error = g_error_new (MCP_ERROR,
                         MCP_ERROR_INVALID_REQUEST,
                         "Message exceeds the %" G_GSIZE_FORMAT
                         "-byte line limit without a newline",
                         self->max_line_bytes);

    g_string_truncate (self->inbound, 0);
    mcp_transport_emit_error (MCP_TRANSPORT (self), error);
    set_state (self, MCP_TRANSPORT_STATE_ERROR);
}

/*
 * drain_inbound:
 *
 * Every whole line in what has arrived so far.
 *
 * Returns: %TRUE if the read loop should be re-armed
 */
static gboolean
drain_inbound (McpStdioTransport *self)
{
    for (;;)
    {
        gchar *newline;
        gsize length;
        g_autofree gchar *line = NULL;

        newline = memchr (self->inbound->str, '\n', self->inbound->len);

        if (newline == NULL)
        {
            if (self->max_line_bytes > 0 &&
                self->inbound->len > self->max_line_bytes)
            {
                refuse_oversized_line (self);
                return FALSE;
            }

            return TRUE;
        }

        length = (gsize) (newline - self->inbound->str);

        /* A \r\n peer.  The old reader was set to LF-only, which left the
         * \r inside the JSON text; json-glib tolerates it as trailing
         * whitespace, but only by luck. */
        if (length > 0 && self->inbound->str[length - 1] == '\r')
        {
            length--;
        }

        line = g_strndup (self->inbound->str, length);
        g_string_erase (self->inbound, 0,
                        (gssize) (newline - self->inbound->str) + 1);

        if (self->max_line_bytes > 0 && length > self->max_line_bytes)
        {
            refuse_oversized_line (self);
            return FALSE;
        }

        /* Blank lines are framing, not messages. */
        if (length == 0)
        {
            continue;
        }

        dispatch_line (self, line, length);

        /* A message-received handler is allowed to disconnect. */
        if (self->state != MCP_TRANSPORT_STATE_CONNECTED)
        {
            return FALSE;
        }
    }
}

static void
read_chunk_cb (GObject      *source,
               GAsyncResult *result,
               gpointer      user_data)
{
    ReadOp *op = user_data;
    g_autoptr(McpStdioTransport) self = g_weak_ref_get (&op->transport);
    g_autoptr(GError) error = NULL;
    gssize got;

    got = g_input_stream_read_finish (G_INPUT_STREAM (source), result, &error);

    /* The transport was finalised while this read was in flight.  Nothing
     * to deliver the bytes to, and nothing to re-arm. */
    if (self == NULL)
    {
        read_op_free (op);
        return;
    }

    if (got < 0)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            mcp_transport_emit_error (MCP_TRANSPORT (self), error);
            set_state (self, MCP_TRANSPORT_STATE_ERROR);
        }
        read_op_free (op);
        return;
    }

    if (got == 0)
    {
        /* EOF - remote end closed connection */
        g_autoptr(GError) eof_error = NULL;

        eof_error = g_error_new (MCP_ERROR,
                                 MCP_ERROR_CONNECTION_CLOSED,
                                 "Remote end closed connection");
        mcp_transport_emit_error (MCP_TRANSPORT (self), eof_error);
        set_state (self, MCP_TRANSPORT_STATE_DISCONNECTED);
        read_op_free (op);
        return;
    }

    g_string_append_len (self->inbound, (const gchar *) op->chunk, got);
    read_op_free (op);

    if (!drain_inbound (self))
    {
        return;
    }

    start_read_loop (self);
}

static void
start_read_loop (McpStdioTransport *self)
{
    ReadOp *op;

    if (self->state != MCP_TRANSPORT_STATE_CONNECTED)
    {
        return;
    }

    if (self->input == NULL)
    {
        return;
    }

    if (self->read_cancellable == NULL ||
        g_cancellable_is_cancelled (self->read_cancellable))
    {
        return;
    }

    op = read_op_new (self);
    g_input_stream_read_async (op->stream,
                               op->chunk,
                               sizeof op->chunk,
                               G_PRIORITY_DEFAULT,
                               self->read_cancellable,
                               read_chunk_cb,
                               op);
}

/*
 * Idle callback to start the read loop.
 * This is used to defer the read loop start until after connect completes.
 * The GSource destroy notify handles g_object_unref.
 */
static gboolean
start_read_loop_idle (gpointer user_data)
{
    McpStdioTransport *self = MCP_STDIO_TRANSPORT (user_data);

    start_read_loop (self);

    return G_SOURCE_REMOVE;
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

    /* Use stdin and stdout - platform-specific stream creation */
#ifdef _WIN32
    self->input = g_win32_input_stream_new (GetStdHandle (STD_INPUT_HANDLE), FALSE);
    self->output = g_win32_output_stream_new (GetStdHandle (STD_OUTPUT_HANDLE), FALSE);
#else
    self->input = g_unix_input_stream_new (STDIN_FILENO, FALSE);
    self->output = g_unix_output_stream_new (STDOUT_FILENO, FALSE);
#endif
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

    /*
     * Unset G_MESSAGES_DEBUG in the subprocess environment.
     * When this variable is set, GLib's structured logging can cause
     * debug messages to be written to stdout, which would corrupt
     * the JSON-RPC protocol stream.
     */
    g_subprocess_launcher_unsetenv (launcher, "G_MESSAGES_DEBUG");

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

/**
 * mcp_stdio_transport_get_max_line_bytes:
 * @self: an #McpStdioTransport
 *
 * Gets the cap on a single newline-delimited message.
 *
 * Returns: the cap in bytes, or 0 if the transport accumulates without a
 *   bound
 */
gsize
mcp_stdio_transport_get_max_line_bytes (McpStdioTransport *self)
{
    g_return_val_if_fail (MCP_IS_STDIO_TRANSPORT (self), 0);

    return self->max_line_bytes;
}

/**
 * mcp_stdio_transport_set_max_line_bytes:
 * @self: an #McpStdioTransport
 * @max_line_bytes: the cap in bytes, or 0 to remove it
 *
 * Sets the cap on a single newline-delimited message.  The cap is applied
 * to the bytes held while looking for the newline, before the next read
 * rather than after it, which is the difference between refusing an
 * over-long message and buffering it first.
 */
void
mcp_stdio_transport_set_max_line_bytes (McpStdioTransport *self,
                                        gsize              max_line_bytes)
{
    g_return_if_fail (MCP_IS_STDIO_TRANSPORT (self));

    if (self->max_line_bytes == max_line_bytes)
    {
        return;
    }

    self->max_line_bytes = max_line_bytes;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_LINE_BYTES]);
}
