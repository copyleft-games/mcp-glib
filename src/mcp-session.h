/*
 * mcp-session.h - Session base class for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the base session class that provides common
 * functionality for both client and server sessions:
 * - Protocol version negotiation
 * - Session state management
 * - Request ID generation
 * - Pending request tracking
 */

#ifndef MCP_SESSION_H
#define MCP_SESSION_H


#include <glib-object.h>
#include <json-glib/json-glib.h>
#include "mcp-enums.h"
#include "mcp-capabilities.h"
#include "mcp-message.h"

G_BEGIN_DECLS

/**
 * McpSessionState:
 * @MCP_SESSION_STATE_DISCONNECTED: Not connected
 * @MCP_SESSION_STATE_CONNECTING: Connection in progress
 * @MCP_SESSION_STATE_INITIALIZING: Connected, performing handshake
 * @MCP_SESSION_STATE_READY: Fully initialized and ready
 * @MCP_SESSION_STATE_CLOSING: Shutdown in progress
 * @MCP_SESSION_STATE_ERROR: Error state
 *
 * The state of an MCP session.
 */
typedef enum
{
    MCP_SESSION_STATE_DISCONNECTED,
    MCP_SESSION_STATE_CONNECTING,
    MCP_SESSION_STATE_INITIALIZING,
    MCP_SESSION_STATE_READY,
    MCP_SESSION_STATE_CLOSING,
    MCP_SESSION_STATE_ERROR
} McpSessionState;

#define MCP_TYPE_SESSION (mcp_session_get_type ())

G_DECLARE_DERIVABLE_TYPE (McpSession, mcp_session, MCP, SESSION, GObject)

/**
 * McpSessionClass:
 * @parent_class: the parent class
 *
 * The class structure for #McpSession.
 */
struct _McpSessionClass
{
    GObjectClass parent_class;

    /*< private >*/
    gpointer padding[8];
};

/**
 * mcp_session_get_state:
 * @self: an #McpSession
 *
 * Gets the current session state.
 *
 * Returns: the #McpSessionState
 */
McpSessionState mcp_session_get_state (McpSession *self);

/**
 * mcp_session_get_protocol_version:
 * @self: an #McpSession
 *
 * Gets the negotiated protocol version.
 * Only valid after initialization is complete.
 *
 * Returns: (transfer none) (nullable): the protocol version, or %NULL
 */
const gchar *mcp_session_get_protocol_version (McpSession *self);

/**
 * mcp_session_get_local_implementation:
 * @self: an #McpSession
 *
 * Gets the local implementation info.
 *
 * Returns: (transfer none) (nullable): the local #McpImplementation
 */
McpImplementation *mcp_session_get_local_implementation (McpSession *self);

/**
 * mcp_session_set_local_implementation:
 * @self: an #McpSession
 * @impl: (transfer none): the implementation info
 *
 * Sets the local implementation info.
 */
void mcp_session_set_local_implementation (McpSession        *self,
                                           McpImplementation *impl);

/**
 * mcp_session_get_remote_implementation:
 * @self: an #McpSession
 *
 * Gets the remote peer's implementation info.
 * Only valid after initialization is complete.
 *
 * Returns: (transfer none) (nullable): the remote #McpImplementation
 */
McpImplementation *mcp_session_get_remote_implementation (McpSession *self);

/**
 * mcp_session_generate_request_id:
 * @self: an #McpSession
 *
 * Generates a unique request ID for this session.
 *
 * Returns: (transfer full): a new request ID string
 */
gchar *mcp_session_generate_request_id (McpSession *self);

/**
 * mcp_session_has_pending_request:
 * @self: an #McpSession
 * @request_id: the request ID to check
 *
 * Checks if there is a pending request with the given ID.
 *
 * Returns: %TRUE if a request with this ID is pending
 */
gboolean mcp_session_has_pending_request (McpSession  *self,
                                          const gchar *request_id);

/**
 * mcp_session_get_pending_request_count:
 * @self: an #McpSession
 *
 * Gets the number of pending requests.
 *
 * Returns: the number of pending requests
 */
guint mcp_session_get_pending_request_count (McpSession *self);

/*
 * Internal functions for subclasses.
 * These are used by McpServer and McpClient.
 */

/**
 * mcp_session_set_state:
 * @self: an #McpSession
 * @new_state: the new state
 *
 * Sets the session state and emits the state-changed signal.
 * This is an internal function for subclasses.
 */
void mcp_session_set_state (McpSession      *self,
                            McpSessionState  new_state);

/**
 * mcp_session_set_protocol_version:
 * @self: an #McpSession
 * @version: the protocol version
 *
 * Sets the negotiated protocol version.
 * This is an internal function for subclasses.
 */
void mcp_session_set_protocol_version (McpSession  *self,
                                       const gchar *version);

/**
 * mcp_session_set_remote_implementation:
 * @self: an #McpSession
 * @impl: (nullable): the remote implementation info
 *
 * Sets the remote peer's implementation info.
 * This is an internal function for subclasses.
 */
void mcp_session_set_remote_implementation (McpSession        *self,
                                            McpImplementation *impl);

/**
 * mcp_session_add_pending_request:
 * @self: an #McpSession
 * @request_id: the request ID
 * @task: the task to associate with this request
 *
 * Adds a pending request for tracking.
 * This is an internal function for subclasses.
 */
void mcp_session_add_pending_request (McpSession  *self,
                                      const gchar *request_id,
                                      GTask       *task);

/**
 * mcp_session_take_pending_request:
 * @self: an #McpSession
 * @request_id: the request ID
 *
 * Takes a pending request task, removing it from tracking.
 * This is an internal function for subclasses.
 *
 * Returns: (transfer full) (nullable): the task, or %NULL if not found
 */
GTask *mcp_session_take_pending_request (McpSession  *self,
                                         const gchar *request_id);

/**
 * mcp_session_cancel_all_pending_requests:
 * @self: an #McpSession
 * @error: (nullable): the error to return to pending requests
 *
 * Cancels all pending requests with the given error.
 * This is an internal function for subclasses.
 */
void mcp_session_cancel_all_pending_requests (McpSession *self,
                                              GError     *error);

G_END_DECLS

#endif /* MCP_SESSION_H */
