/*
 * mcp-transport.h - Transport interface for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the transport interface that abstracts the communication
 * layer for MCP. Implementations include:
 * - McpStdioTransport: stdin/stdout for subprocess communication
 * - McpHttpTransport: HTTP POST + SSE for network communication
 * - McpWebSocketTransport: WebSocket for bidirectional network communication
 */

#ifndef MCP_TRANSPORT_H
#define MCP_TRANSPORT_H


#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include "mcp-enums.h"

G_BEGIN_DECLS

/* McpTransportState is defined in mcp-enums.h */

#define MCP_TYPE_TRANSPORT (mcp_transport_get_type ())

G_DECLARE_INTERFACE (McpTransport, mcp_transport, MCP, TRANSPORT, GObject)

/**
 * McpTransportInterface:
 * @parent_iface: the parent interface
 * @get_state: gets the current transport state
 * @connect_async: initiates an asynchronous connection
 * @connect_finish: completes an asynchronous connection
 * @disconnect_async: initiates an asynchronous disconnection
 * @disconnect_finish: completes an asynchronous disconnection
 * @send_message_async: sends a message asynchronously
 * @send_message_finish: completes an asynchronous send
 *
 * The interface structure for #McpTransport implementations.
 */
struct _McpTransportInterface
{
    GTypeInterface parent_iface;

    /*< public >*/

    /**
     * McpTransportInterface::get_state:
     * @self: an #McpTransport
     *
     * Gets the current transport state.
     *
     * Returns: the #McpTransportState
     */
    McpTransportState (*get_state) (McpTransport *self);

    /**
     * McpTransportInterface::connect_async:
     * @self: an #McpTransport
     * @cancellable: (nullable): a #GCancellable
     * @callback: (scope async): callback to call when complete
     * @user_data: (closure): user data for @callback
     *
     * Initiates an asynchronous connection.
     */
    void (*connect_async) (McpTransport        *self,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data);

    /**
     * McpTransportInterface::connect_finish:
     * @self: an #McpTransport
     * @result: the #GAsyncResult
     * @error: (nullable): return location for a #GError
     *
     * Completes an asynchronous connection.
     *
     * Returns: %TRUE on success, %FALSE on error
     */
    gboolean (*connect_finish) (McpTransport  *self,
                                GAsyncResult  *result,
                                GError       **error);

    /**
     * McpTransportInterface::disconnect_async:
     * @self: an #McpTransport
     * @cancellable: (nullable): a #GCancellable
     * @callback: (scope async): callback to call when complete
     * @user_data: (closure): user data for @callback
     *
     * Initiates an asynchronous disconnection.
     */
    void (*disconnect_async) (McpTransport        *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data);

    /**
     * McpTransportInterface::disconnect_finish:
     * @self: an #McpTransport
     * @result: the #GAsyncResult
     * @error: (nullable): return location for a #GError
     *
     * Completes an asynchronous disconnection.
     *
     * Returns: %TRUE on success, %FALSE on error
     */
    gboolean (*disconnect_finish) (McpTransport  *self,
                                   GAsyncResult  *result,
                                   GError       **error);

    /**
     * McpTransportInterface::send_message_async:
     * @self: an #McpTransport
     * @message: the JSON message to send
     * @cancellable: (nullable): a #GCancellable
     * @callback: (scope async): callback to call when complete
     * @user_data: (closure): user data for @callback
     *
     * Sends a JSON message asynchronously.
     */
    void (*send_message_async) (McpTransport        *self,
                                JsonNode            *message,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data);

    /**
     * McpTransportInterface::send_message_finish:
     * @self: an #McpTransport
     * @result: the #GAsyncResult
     * @error: (nullable): return location for a #GError
     *
     * Completes an asynchronous message send.
     *
     * Returns: %TRUE on success, %FALSE on error
     */
    gboolean (*send_message_finish) (McpTransport  *self,
                                     GAsyncResult  *result,
                                     GError       **error);

    /*< private >*/
    gpointer padding[8];
};

/**
 * mcp_transport_get_state:
 * @self: an #McpTransport
 *
 * Gets the current transport state.
 *
 * Returns: the #McpTransportState
 */
McpTransportState mcp_transport_get_state (McpTransport *self);

/**
 * mcp_transport_connect_async:
 * @self: an #McpTransport
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Initiates an asynchronous connection.
 */
void mcp_transport_connect_async (McpTransport        *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data);

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
gboolean mcp_transport_connect_finish (McpTransport  *self,
                                       GAsyncResult  *result,
                                       GError       **error);

/**
 * mcp_transport_disconnect_async:
 * @self: an #McpTransport
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Initiates an asynchronous disconnection.
 */
void mcp_transport_disconnect_async (McpTransport        *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data);

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
gboolean mcp_transport_disconnect_finish (McpTransport  *self,
                                          GAsyncResult  *result,
                                          GError       **error);

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
void mcp_transport_send_message_async (McpTransport        *self,
                                       JsonNode            *message,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data);

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
gboolean mcp_transport_send_message_finish (McpTransport  *self,
                                            GAsyncResult  *result,
                                            GError       **error);

/**
 * mcp_transport_is_connected:
 * @self: an #McpTransport
 *
 * Checks if the transport is currently connected.
 *
 * Returns: %TRUE if connected, %FALSE otherwise
 */
gboolean mcp_transport_is_connected (McpTransport *self);

/*
 * Helper functions for transport implementations to emit signals.
 * These should only be called by McpTransport implementations.
 */

/**
 * mcp_transport_emit_message_received:
 * @self: an #McpTransport
 * @message: (transfer none): the received message
 *
 * Emits the "message-received" signal.
 * This function is for use by transport implementations.
 */
void mcp_transport_emit_message_received (McpTransport *self,
                                          JsonNode     *message);

/**
 * mcp_transport_emit_state_changed:
 * @self: an #McpTransport
 * @old_state: the previous state
 * @new_state: the new state
 *
 * Emits the "state-changed" signal.
 * This function is for use by transport implementations.
 */
void mcp_transport_emit_state_changed (McpTransport      *self,
                                       McpTransportState  old_state,
                                       McpTransportState  new_state);

/**
 * mcp_transport_emit_error:
 * @self: an #McpTransport
 * @error: (transfer none): the error
 *
 * Emits the "error" signal.
 * This function is for use by transport implementations.
 */
void mcp_transport_emit_error (McpTransport *self,
                               GError       *error);

G_END_DECLS

#endif /* MCP_TRANSPORT_H */
