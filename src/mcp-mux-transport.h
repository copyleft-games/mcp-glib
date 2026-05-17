/*
 * mcp-mux-transport.h - Multiplexed (transport-agnostic) transport for mcp-glib
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * McpMuxTransport is an McpTransport implementation that owns no I/O of
 * its own.  Outbound JSON frames are handed to a user-supplied callback
 * (typically a host that multiplexes MCP frames alongside other traffic
 * on a shared wire — e.g. a chat-bridge WebSocket).  Inbound frames are
 * pushed in by the host via mcp_mux_transport_dispatch_frame().
 *
 * This lets any McpClient or McpServer ride on top of an arbitrary
 * carrier without that carrier having to know anything about MCP.
 */

#ifndef MCP_MUX_TRANSPORT_H
#define MCP_MUX_TRANSPORT_H

#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include "mcp-transport.h"

G_BEGIN_DECLS

#define MCP_TYPE_MUX_TRANSPORT (mcp_mux_transport_get_type ())

G_DECLARE_FINAL_TYPE (McpMuxTransport, mcp_mux_transport,
                      MCP, MUX_TRANSPORT, GObject)

/**
 * McpMuxSendFunc:
 * @self: the #McpMuxTransport that wants to send
 * @frame: (transfer none): the JSON frame to ship outbound
 * @user_data: (closure): user data registered with the callback
 *
 * Called from mcp_transport_send_message_async() to push an outbound
 * MCP frame onto the host's carrier.  The callback must not free
 * @frame.  Errors raised by the host should be reported back to the
 * transport via mcp_mux_transport_emit_error().
 */
typedef void (*McpMuxSendFunc) (McpMuxTransport *self,
                                JsonNode        *frame,
                                gpointer         user_data);

/**
 * mcp_mux_transport_new:
 * @context: (nullable): a #GMainContext for dispatching inbound frames,
 *   or %NULL to use the thread-default context at construction time
 *
 * Creates a new mux transport.  The transport starts in the
 * %MCP_TRANSPORT_STATE_DISCONNECTED state.  Hosts must call
 * mcp_mux_transport_set_send_callback() before any
 * mcp_transport_send_message_async() call, and
 * mcp_mux_transport_set_connected() once the underlying carrier is
 * ready to accept frames.
 *
 * Returns: (transfer full): a new #McpMuxTransport
 */
McpMuxTransport *mcp_mux_transport_new (GMainContext *context);

/**
 * mcp_mux_transport_set_send_callback:
 * @self: an #McpMuxTransport
 * @callback: (scope notified) (nullable): the callback to invoke when
 *   the transport wants to send a frame, or %NULL to unset
 * @user_data: (closure): user data for @callback
 * @destroy: (nullable): destroy notify for @user_data
 *
 * Registers the outbound dispatch callback.  Replacing an existing
 * callback runs its destroy notify.  Passing %NULL clears the
 * callback; subsequent sends will fail with %MCP_ERROR_TRANSPORT.
 */
void mcp_mux_transport_set_send_callback (McpMuxTransport *self,
                                          McpMuxSendFunc   callback,
                                          gpointer         user_data,
                                          GDestroyNotify   destroy);

/**
 * mcp_mux_transport_dispatch_frame:
 * @self: an #McpMuxTransport
 * @frame: (transfer none): a JSON frame received from the carrier
 *
 * Pushes a received frame into the transport.  The "message-received"
 * signal is emitted on the transport's #GMainContext (so callers from
 * a worker thread or libsoup callback context don't need to marshal
 * themselves).  The frame is reffed before being scheduled, so the
 * caller may free its reference after returning.
 *
 * If the transport is not in the %MCP_TRANSPORT_STATE_CONNECTED state
 * the frame is silently dropped.
 */
void mcp_mux_transport_dispatch_frame (McpMuxTransport *self,
                                       JsonNode        *frame);

/**
 * mcp_mux_transport_set_connected:
 * @self: an #McpMuxTransport
 * @connected: %TRUE to mark the transport as ready, %FALSE to mark
 *   it as disconnected
 *
 * Flips the connection state of the transport and emits the
 * appropriate "state-changed" transition.  This is intended to be
 * called by the host as the underlying carrier becomes ready or tears
 * down.
 */
void mcp_mux_transport_set_connected (McpMuxTransport *self,
                                      gboolean         connected);

/**
 * mcp_mux_transport_emit_error_from_host:
 * @self: an #McpMuxTransport
 * @error: (transfer none): the error to report
 *
 * Convenience wrapper around mcp_transport_emit_error() that the host
 * can call when its carrier encounters a fault that affects this
 * transport.  Provided separately from the interface helper so hosts
 * don't need to cast through the McpTransport ABI.
 */
void mcp_mux_transport_emit_error_from_host (McpMuxTransport *self,
                                             GError          *error);

/**
 * mcp_mux_transport_get_context:
 * @self: an #McpMuxTransport
 *
 * Returns the #GMainContext used to dispatch inbound frames.
 *
 * Returns: (transfer none): the context (never %NULL)
 */
GMainContext *mcp_mux_transport_get_context (McpMuxTransport *self);

G_END_DECLS

#endif /* MCP_MUX_TRANSPORT_H */
