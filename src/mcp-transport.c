/*
 * mcp-transport.c - Transport interface implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp-transport.h"
#include "mcp-error.h"
#undef MCP_COMPILATION

/**
 * SECTION:mcp-transport
 * @title: McpTransport
 * @short_description: Transport interface for MCP communication
 *
 * #McpTransport is an interface that abstracts the communication layer for MCP.
 * Implementations provide the actual transport mechanism (stdio, HTTP, WebSocket).
 *
 * Signals:
 * - "message-received": Emitted when a JSON message is received
 * - "state-changed": Emitted when the transport state changes
 * - "error": Emitted when a transport error occurs
 */

enum
{
    SIGNAL_MESSAGE_RECEIVED,
    SIGNAL_STATE_CHANGED,
    SIGNAL_ERROR,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_INTERFACE (McpTransport, mcp_transport, G_TYPE_OBJECT)

static void
mcp_transport_default_init (McpTransportInterface *iface)
{
    /**
     * McpTransport::message-received:
     * @self: the #McpTransport
     * @message: (transfer none): the received #JsonNode message
     *
     * Emitted when a JSON message is received from the remote peer.
     * The message is owned by the transport and should not be freed.
     */
    signals[SIGNAL_MESSAGE_RECEIVED] =
        g_signal_new ("message-received",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE,
                      1,
                      JSON_TYPE_NODE);

    /**
     * McpTransport::state-changed:
     * @self: the #McpTransport
     * @old_state: the previous #McpTransportState
     * @new_state: the new #McpTransportState
     *
     * Emitted when the transport state changes.
     */
    signals[SIGNAL_STATE_CHANGED] =
        g_signal_new ("state-changed",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE,
                      2,
                      G_TYPE_INT,
                      G_TYPE_INT);

    /**
     * McpTransport::error:
     * @self: the #McpTransport
     * @error: (transfer none): the #GError describing the error
     *
     * Emitted when a transport error occurs.
     */
    signals[SIGNAL_ERROR] =
        g_signal_new ("error",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_ERROR);
}

/**
 * mcp_transport_get_state:
 * @self: an #McpTransport
 *
 * Gets the current transport state.
 *
 * Returns: the #McpTransportState
 */
McpTransportState
mcp_transport_get_state (McpTransport *self)
{
    McpTransportInterface *iface;

    g_return_val_if_fail (MCP_IS_TRANSPORT (self), MCP_TRANSPORT_STATE_DISCONNECTED);

    iface = MCP_TRANSPORT_GET_IFACE (self);
    g_return_val_if_fail (iface->get_state != NULL, MCP_TRANSPORT_STATE_DISCONNECTED);

    return iface->get_state (self);
}

/**
 * mcp_transport_connect_async:
 * @self: an #McpTransport
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Initiates an asynchronous connection.
 */
void
mcp_transport_connect_async (McpTransport        *self,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    McpTransportInterface *iface;

    g_return_if_fail (MCP_IS_TRANSPORT (self));

    iface = MCP_TRANSPORT_GET_IFACE (self);
    g_return_if_fail (iface->connect_async != NULL);

    iface->connect_async (self, cancellable, callback, user_data);
}

/**
 * mcp_transport_connect_finish:
 * @self: an #McpTransport
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous connection.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
mcp_transport_connect_finish (McpTransport  *self,
                              GAsyncResult  *result,
                              GError       **error)
{
    McpTransportInterface *iface;

    g_return_val_if_fail (MCP_IS_TRANSPORT (self), FALSE);
    g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

    iface = MCP_TRANSPORT_GET_IFACE (self);
    g_return_val_if_fail (iface->connect_finish != NULL, FALSE);

    return iface->connect_finish (self, result, error);
}

/**
 * mcp_transport_disconnect_async:
 * @self: an #McpTransport
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Initiates an asynchronous disconnection.
 */
void
mcp_transport_disconnect_async (McpTransport        *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
    McpTransportInterface *iface;

    g_return_if_fail (MCP_IS_TRANSPORT (self));

    iface = MCP_TRANSPORT_GET_IFACE (self);
    g_return_if_fail (iface->disconnect_async != NULL);

    iface->disconnect_async (self, cancellable, callback, user_data);
}

/**
 * mcp_transport_disconnect_finish:
 * @self: an #McpTransport
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous disconnection.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
mcp_transport_disconnect_finish (McpTransport  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
    McpTransportInterface *iface;

    g_return_val_if_fail (MCP_IS_TRANSPORT (self), FALSE);
    g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

    iface = MCP_TRANSPORT_GET_IFACE (self);
    g_return_val_if_fail (iface->disconnect_finish != NULL, FALSE);

    return iface->disconnect_finish (self, result, error);
}

/**
 * mcp_transport_send_message_async:
 * @self: an #McpTransport
 * @message: (transfer none): the JSON message to send
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Sends a JSON message asynchronously.
 */
void
mcp_transport_send_message_async (McpTransport        *self,
                                  JsonNode            *message,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
    McpTransportInterface *iface;

    g_return_if_fail (MCP_IS_TRANSPORT (self));
    g_return_if_fail (message != NULL);

    iface = MCP_TRANSPORT_GET_IFACE (self);
    g_return_if_fail (iface->send_message_async != NULL);

    iface->send_message_async (self, message, cancellable, callback, user_data);
}

/**
 * mcp_transport_send_message_finish:
 * @self: an #McpTransport
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous message send.
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
mcp_transport_send_message_finish (McpTransport  *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
    McpTransportInterface *iface;

    g_return_val_if_fail (MCP_IS_TRANSPORT (self), FALSE);
    g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

    iface = MCP_TRANSPORT_GET_IFACE (self);
    g_return_val_if_fail (iface->send_message_finish != NULL, FALSE);

    return iface->send_message_finish (self, result, error);
}

/**
 * mcp_transport_is_connected:
 * @self: an #McpTransport
 *
 * Checks if the transport is currently connected.
 *
 * Returns: %TRUE if connected, %FALSE otherwise
 */
gboolean
mcp_transport_is_connected (McpTransport *self)
{
    g_return_val_if_fail (MCP_IS_TRANSPORT (self), FALSE);

    return mcp_transport_get_state (self) == MCP_TRANSPORT_STATE_CONNECTED;
}

/*
 * Helper functions for implementations to emit signals.
 */

/**
 * mcp_transport_emit_message_received:
 * @self: an #McpTransport
 * @message: (transfer none): the received message
 *
 * Emits the "message-received" signal.
 * This function is for use by transport implementations.
 */
void
mcp_transport_emit_message_received (McpTransport *self,
                                     JsonNode     *message)
{
    g_return_if_fail (MCP_IS_TRANSPORT (self));
    g_return_if_fail (message != NULL);

    g_signal_emit (self, signals[SIGNAL_MESSAGE_RECEIVED], 0, message);
}

/**
 * mcp_transport_emit_state_changed:
 * @self: an #McpTransport
 * @old_state: the previous state
 * @new_state: the new state
 *
 * Emits the "state-changed" signal.
 * This function is for use by transport implementations.
 */
void
mcp_transport_emit_state_changed (McpTransport      *self,
                                  McpTransportState  old_state,
                                  McpTransportState  new_state)
{
    g_return_if_fail (MCP_IS_TRANSPORT (self));

    g_signal_emit (self, signals[SIGNAL_STATE_CHANGED], 0, old_state, new_state);
}

/**
 * mcp_transport_emit_error:
 * @self: an #McpTransport
 * @error: (transfer none): the error
 *
 * Emits the "error" signal.
 * This function is for use by transport implementations.
 */
void
mcp_transport_emit_error (McpTransport *self,
                          GError       *error)
{
    g_return_if_fail (MCP_IS_TRANSPORT (self));
    g_return_if_fail (error != NULL);

    g_signal_emit (self, signals[SIGNAL_ERROR], 0, error);
}
