/*
 * mcp-tool-call.c - A tools/call request answered later
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * An McpToolCall belongs to the deferred tool handler that received it
 * (and to the server's table of calls in flight, until it completes or
 * is cancelled).  It holds its server weakly: a server that went away
 * leaves a call that completes into nothing, which is what a handler
 * finishing late after a disconnect should see.
 */

#include "mcp-tool-call.h"
#include "mcp-tool-call-private.h"
#include "mcp-error.h"

struct _McpToolCall
{
    GObject       parent_instance;

    GWeakRef      server;          /* McpServer */
    McpRequest   *request;         /* the tools/call request */
    gchar        *name;
    JsonObject   *arguments;       /* nullable */
    JsonNode     *progress_token;  /* nullable: a string or a number */
    GCancellable *cancellable;
    gboolean      completed;
    gboolean      cancelled;
};

G_DEFINE_FINAL_TYPE (McpToolCall, mcp_tool_call, G_TYPE_OBJECT)

enum
{
    SIGNAL_CANCELLED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
mcp_tool_call_finalize (GObject *object)
{
    McpToolCall *self = MCP_TOOL_CALL (object);

    g_weak_ref_clear (&self->server);
    g_clear_object (&self->request);
    g_clear_pointer (&self->name, g_free);
    g_clear_pointer (&self->arguments, json_object_unref);
    g_clear_pointer (&self->progress_token, json_node_unref);
    g_clear_object (&self->cancellable);

    G_OBJECT_CLASS (mcp_tool_call_parent_class)->finalize (object);
}

static void
mcp_tool_call_class_init (McpToolCallClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = mcp_tool_call_finalize;

    /**
     * McpToolCall::cancelled:
     * @self: the #McpToolCall
     *
     * Emitted when the client cancels the request
     * (notifications/cancelled) or the connection closes before the call
     * completed.  Stop the work; nothing returned afterwards reaches the
     * client.
     */
    signals[SIGNAL_CANCELLED] =
        g_signal_new ("cancelled",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 0);
}

static void
mcp_tool_call_init (McpToolCall *self)
{
    g_weak_ref_init (&self->server, NULL);
    self->cancellable = g_cancellable_new ();
}

McpToolCall *
_mcp_tool_call_new (McpServer   *server,
                    McpRequest  *request,
                    const gchar *name,
                    JsonObject  *arguments,
                    JsonNode    *progress_token)
{
    McpToolCall *self;

    g_return_val_if_fail (MCP_IS_SERVER (server), NULL);
    g_return_val_if_fail (MCP_IS_REQUEST (request), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    self = g_object_new (MCP_TYPE_TOOL_CALL, NULL);
    g_weak_ref_set (&self->server, server);
    self->request = g_object_ref (request);
    self->name = g_strdup (name);
    self->arguments = arguments != NULL ? json_object_ref (arguments) : NULL;
    self->progress_token = progress_token != NULL ? json_node_copy (progress_token) : NULL;
    return self;
}

McpRequest *
_mcp_tool_call_get_request (McpToolCall *self)
{
    g_return_val_if_fail (MCP_IS_TOOL_CALL (self), NULL);

    return self->request;
}

void
_mcp_tool_call_cancel (McpToolCall *self)
{
    g_return_if_fail (MCP_IS_TOOL_CALL (self));

    if (self->completed || self->cancelled)
    {
        return;
    }
    self->cancelled = TRUE;
    g_cancellable_cancel (self->cancellable);
    g_signal_emit (self, signals[SIGNAL_CANCELLED], 0);
}

const gchar *
mcp_tool_call_get_name (McpToolCall *self)
{
    g_return_val_if_fail (MCP_IS_TOOL_CALL (self), NULL);

    return self->name;
}

JsonObject *
mcp_tool_call_get_arguments (McpToolCall *self)
{
    g_return_val_if_fail (MCP_IS_TOOL_CALL (self), NULL);

    return self->arguments;
}

const gchar *
mcp_tool_call_get_request_id (McpToolCall *self)
{
    g_return_val_if_fail (MCP_IS_TOOL_CALL (self), NULL);

    return mcp_request_get_id (self->request);
}

gboolean
mcp_tool_call_has_progress_token (McpToolCall *self)
{
    g_return_val_if_fail (MCP_IS_TOOL_CALL (self), FALSE);

    return self->progress_token != NULL;
}

GCancellable *
mcp_tool_call_get_cancellable (McpToolCall *self)
{
    g_return_val_if_fail (MCP_IS_TOOL_CALL (self), NULL);

    return self->cancellable;
}

gboolean
mcp_tool_call_is_completed (McpToolCall *self)
{
    g_return_val_if_fail (MCP_IS_TOOL_CALL (self), FALSE);

    return self->completed;
}

gboolean
mcp_tool_call_is_cancelled (McpToolCall *self)
{
    g_return_val_if_fail (MCP_IS_TOOL_CALL (self), FALSE);

    return self->cancelled;
}

void
mcp_tool_call_send_progress (McpToolCall *self,
                             gdouble      progress,
                             gdouble      total,
                             const gchar *message)
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(JsonBuilder) builder = NULL;

    g_return_if_fail (MCP_IS_TOOL_CALL (self));

    if (self->progress_token == NULL || self->completed || self->cancelled)
    {
        return;
    }
    server = g_weak_ref_get (&self->server);
    if (server == NULL)
    {
        return;
    }

    /* the token goes back exactly as the client sent it: string or number */
    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "progressToken");
    json_builder_add_value (builder, json_node_copy (self->progress_token));
    json_builder_set_member_name (builder, "progress");
    json_builder_add_double_value (builder, progress);
    if (total > 0.0)
    {
        json_builder_set_member_name (builder, "total");
        json_builder_add_double_value (builder, total);
    }
    if (message != NULL)
    {
        json_builder_set_member_name (builder, "message");
        json_builder_add_string_value (builder, message);
    }
    json_builder_end_object (builder);
    _mcp_server_send_tool_call_progress (server, json_builder_get_root (builder));
}

/* the one way a call completes: marks it, then lets the server answer */
static void
complete (McpToolCall *self,
          JsonNode    *result,
          gint         error_code,
          const gchar *error_message)
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(JsonNode) owned = result;
    g_autoptr(McpToolCall) keep = NULL;

    if (self->completed)
    {
        return;
    }
    self->completed = TRUE;
    /* the protocol forbids answering a request the client cancelled */
    if (self->cancelled)
    {
        return;
    }
    server = g_weak_ref_get (&self->server);
    if (server == NULL)
    {
        return;
    }
    keep = g_object_ref (self);
    _mcp_server_finish_tool_call (server, self, g_steal_pointer (&owned), error_code, error_message);
}

void
mcp_tool_call_return_result (McpToolCall   *self,
                             McpToolResult *result)
{
    g_return_if_fail (MCP_IS_TOOL_CALL (self));
    g_return_if_fail (result != NULL);

    if (self->completed)
    {
        return;
    }
    complete (self, mcp_tool_result_to_json (result), 0, NULL);
}

void
mcp_tool_call_return_error (McpToolCall *self,
                            gint         code,
                            const gchar *message)
{
    g_return_if_fail (MCP_IS_TOOL_CALL (self));
    g_return_if_fail (message != NULL);

    if (self->completed)
    {
        return;
    }
    complete (self, NULL, code, message);
}
