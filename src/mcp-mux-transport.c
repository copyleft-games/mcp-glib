/*
 * mcp-mux-transport.c - Multiplexed transport implementation
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "mcp-mux-transport.h"
#include "mcp-error.h"

#include <gio/gio.h>

/**
 * SECTION:mcp-mux-transport
 * @title: McpMuxTransport
 * @short_description: I/O-free transport that delegates framing to a host
 *
 * #McpMuxTransport carries MCP frames over a host-supplied carrier.
 * The carrier (often a chat-bridge WebSocket) calls
 * mcp_mux_transport_dispatch_frame() for every inbound MCP frame it
 * decodes, and the transport calls back into the carrier via the
 * #McpMuxSendFunc registered with
 * mcp_mux_transport_set_send_callback() whenever an MCP layer wants
 * to ship a frame outbound.
 */

struct _McpMuxTransport
{
    GObject parent_instance;

    GMainContext      *context;          /* dispatch context (ref'd)  */
    McpTransportState  state;

    GMutex             lock;             /* guards send_cb + state    */
    McpMuxSendFunc     send_cb;
    gpointer           send_cb_data;
    GDestroyNotify     send_cb_destroy;
};

static void mcp_mux_transport_iface_init (McpTransportInterface *iface);

G_DEFINE_TYPE_WITH_CODE (McpMuxTransport, mcp_mux_transport, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MCP_TYPE_TRANSPORT,
                                                mcp_mux_transport_iface_init))

/* ── state helpers ─────────────────────────────────────────────────── */

/* Caller MUST NOT hold self->lock. */
static void
set_state (McpMuxTransport   *self,
           McpTransportState  new_state)
{
    McpTransportState old_state;

    g_mutex_lock (&self->lock);
    old_state = self->state;
    if (old_state == new_state)
    {
        g_mutex_unlock (&self->lock);
        return;
    }
    self->state = new_state;
    g_mutex_unlock (&self->lock);

    mcp_transport_emit_state_changed (MCP_TRANSPORT (self),
                                      old_state,
                                      new_state);
}

/* ── interface vtable ──────────────────────────────────────────────── */

static McpTransportState
mux_get_state (McpTransport *transport)
{
    McpMuxTransport *self = MCP_MUX_TRANSPORT (transport);
    McpTransportState s;

    g_mutex_lock (&self->lock);
    s = self->state;
    g_mutex_unlock (&self->lock);
    return s;
}

static void
mux_connect_async (McpTransport        *transport,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
    McpMuxTransport *self = MCP_MUX_TRANSPORT (transport);
    g_autoptr(GTask) task = g_task_new (self, cancellable, callback, user_data);

    g_task_set_source_tag (task, mux_connect_async);

    /* The mux transport has no real connection of its own — the host
     * is responsible for getting the carrier ready and calling
     * mcp_mux_transport_set_connected().  We only mark ourselves
     * CONNECTED here if the host hasn't already done so, so the
     * McpClient/McpServer handshake can proceed.  If the host
     * explicitly set us DISCONNECTED before this is called, leave
     * the state alone and fail. */

    g_mutex_lock (&self->lock);
    {
        McpTransportState s = self->state;
        g_mutex_unlock (&self->lock);

        if (s == MCP_TRANSPORT_STATE_CONNECTED)
        {
            g_task_return_boolean (task, TRUE);
            return;
        }
        if (s == MCP_TRANSPORT_STATE_ERROR)
        {
            g_task_return_new_error (task, MCP_ERROR,
                                     MCP_ERROR_TRANSPORT_ERROR,
                                     "mux transport in error state");
            return;
        }
        /* DISCONNECTED / CONNECTING / DISCONNECTING — assume the host
         * will flip us to CONNECTED imminently.  Optimistically
         * advance state ourselves so peers don't stall. */
    }

    set_state (self, MCP_TRANSPORT_STATE_CONNECTED);
    g_task_return_boolean (task, TRUE);
}

static gboolean
mux_connect_finish (McpTransport  *transport,
                    GAsyncResult  *result,
                    GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mux_disconnect_async (McpTransport        *transport,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
    McpMuxTransport *self = MCP_MUX_TRANSPORT (transport);
    g_autoptr(GTask) task = g_task_new (self, cancellable, callback, user_data);

    g_task_set_source_tag (task, mux_disconnect_async);
    set_state (self, MCP_TRANSPORT_STATE_DISCONNECTED);
    g_task_return_boolean (task, TRUE);
}

static gboolean
mux_disconnect_finish (McpTransport  *transport,
                       GAsyncResult  *result,
                       GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mux_send_message_async (McpTransport        *transport,
                        JsonNode            *message,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
    McpMuxTransport *self = MCP_MUX_TRANSPORT (transport);
    g_autoptr(GTask) task = g_task_new (self, cancellable, callback, user_data);
    McpMuxSendFunc cb;
    gpointer       cb_data;
    McpTransportState s;

    g_task_set_source_tag (task, mux_send_message_async);

    g_mutex_lock (&self->lock);
    cb      = self->send_cb;
    cb_data = self->send_cb_data;
    s       = self->state;
    g_mutex_unlock (&self->lock);

    if (s != MCP_TRANSPORT_STATE_CONNECTED)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                 "mux transport not connected");
        return;
    }
    if (cb == NULL)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                 "no send callback registered");
        return;
    }

    /* Invoke the host callback inline.  Hosts that need to defer
     * to a worker thread can do that themselves; doing it here would
     * obscure error reporting and complicate the contract. */
    cb (self, message, cb_data);
    g_task_return_boolean (task, TRUE);
}

static gboolean
mux_send_message_finish (McpTransport  *transport,
                         GAsyncResult  *result,
                         GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mcp_mux_transport_iface_init (McpTransportInterface *iface)
{
    iface->get_state            = mux_get_state;
    iface->connect_async        = mux_connect_async;
    iface->connect_finish       = mux_connect_finish;
    iface->disconnect_async     = mux_disconnect_async;
    iface->disconnect_finish    = mux_disconnect_finish;
    iface->send_message_async   = mux_send_message_async;
    iface->send_message_finish  = mux_send_message_finish;
}

/* ── lifecycle ─────────────────────────────────────────────────────── */

static void
mcp_mux_transport_finalize (GObject *object)
{
    McpMuxTransport *self = MCP_MUX_TRANSPORT (object);

    if (self->send_cb_destroy != NULL && self->send_cb_data != NULL)
        self->send_cb_destroy (self->send_cb_data);
    self->send_cb         = NULL;
    self->send_cb_data    = NULL;
    self->send_cb_destroy = NULL;

    g_clear_pointer (&self->context, g_main_context_unref);
    g_mutex_clear (&self->lock);

    G_OBJECT_CLASS (mcp_mux_transport_parent_class)->finalize (object);
}

static void
mcp_mux_transport_class_init (McpMuxTransportClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = mcp_mux_transport_finalize;
}

static void
mcp_mux_transport_init (McpMuxTransport *self)
{
    g_mutex_init (&self->lock);
    self->state           = MCP_TRANSPORT_STATE_DISCONNECTED;
    self->send_cb         = NULL;
    self->send_cb_data    = NULL;
    self->send_cb_destroy = NULL;
    self->context         = NULL;
}

/* ── public API ────────────────────────────────────────────────────── */

McpMuxTransport *
mcp_mux_transport_new (GMainContext *context)
{
    McpMuxTransport *self;

    self = g_object_new (MCP_TYPE_MUX_TRANSPORT, NULL);
    if (context == NULL)
        context = g_main_context_ref_thread_default ();
    else
        g_main_context_ref (context);
    self->context = context;
    return self;
}

void
mcp_mux_transport_set_send_callback (McpMuxTransport *self,
                                     McpMuxSendFunc   callback,
                                     gpointer         user_data,
                                     GDestroyNotify   destroy)
{
    McpMuxSendFunc old_cb;
    gpointer       old_data;
    GDestroyNotify old_destroy;

    g_return_if_fail (MCP_IS_MUX_TRANSPORT (self));

    g_mutex_lock (&self->lock);
    old_cb              = self->send_cb;
    old_data            = self->send_cb_data;
    old_destroy         = self->send_cb_destroy;
    self->send_cb       = callback;
    self->send_cb_data  = user_data;
    self->send_cb_destroy = destroy;
    g_mutex_unlock (&self->lock);

    /* Run the replaced callback's destroy notify outside the lock
     * so it can safely re-enter the transport. */
    (void) old_cb;
    if (old_destroy != NULL && old_data != NULL)
        old_destroy (old_data);
}

typedef struct
{
    McpMuxTransport *transport;     /* ref */
    JsonNode        *frame;          /* owned */
} DispatchInvocation;

static gboolean
dispatch_invocation_run (gpointer user_data)
{
    DispatchInvocation *inv = user_data;

    if (mcp_transport_get_state (MCP_TRANSPORT (inv->transport))
        == MCP_TRANSPORT_STATE_CONNECTED)
    {
        mcp_transport_emit_message_received (MCP_TRANSPORT (inv->transport),
                                             inv->frame);
    }
    return G_SOURCE_REMOVE;
}

static void
dispatch_invocation_free (gpointer user_data)
{
    DispatchInvocation *inv = user_data;

    g_clear_pointer (&inv->frame, json_node_unref);
    g_clear_object (&inv->transport);
    g_free (inv);
}

void
mcp_mux_transport_dispatch_frame (McpMuxTransport *self,
                                  JsonNode        *frame)
{
    DispatchInvocation *inv;

    g_return_if_fail (MCP_IS_MUX_TRANSPORT (self));
    g_return_if_fail (frame != NULL);

    inv = g_new0 (DispatchInvocation, 1);
    inv->transport = g_object_ref (self);
    inv->frame     = json_node_ref (frame);

    /* Always trampoline onto the transport's GMainContext so callers
     * from arbitrary threads (libsoup callback, GStreamer pad probe…)
     * don't need their own marshalling, and so signal handlers can
     * assume they run on the context the host expects. */
    g_main_context_invoke_full (self->context,
                                G_PRIORITY_DEFAULT,
                                dispatch_invocation_run,
                                inv,
                                dispatch_invocation_free);
}

void
mcp_mux_transport_set_connected (McpMuxTransport *self,
                                 gboolean         connected)
{
    g_return_if_fail (MCP_IS_MUX_TRANSPORT (self));

    set_state (self,
               connected ? MCP_TRANSPORT_STATE_CONNECTED
                         : MCP_TRANSPORT_STATE_DISCONNECTED);
}

void
mcp_mux_transport_emit_error_from_host (McpMuxTransport *self,
                                        GError          *error)
{
    g_return_if_fail (MCP_IS_MUX_TRANSPORT (self));
    g_return_if_fail (error != NULL);

    mcp_transport_emit_error (MCP_TRANSPORT (self), error);
}

GMainContext *
mcp_mux_transport_get_context (McpMuxTransport *self)
{
    g_return_val_if_fail (MCP_IS_MUX_TRANSPORT (self), NULL);
    return self->context;
}
