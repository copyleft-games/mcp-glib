/* Internal outbound request completion and cancellation ownership. */
#ifndef MCP_REQUEST_UTIL_H
#define MCP_REQUEST_UTIL_H

#include "mcp-session.h"
#include "mcp-transport.h"

typedef GTask *(*McpTakeRequest) (McpSession *session, const gchar *id);

typedef struct
{
    GWeakRef session;
    gchar *id;
    McpTakeRequest take;
    gint refs;
} McpPendingSend;

static void
mcp_pending_send_unref (gpointer data)
{
    McpPendingSend *send = data;
    if (!g_atomic_int_dec_and_test (&send->refs))
        return;
    g_weak_ref_clear (&send->session);
    g_free (send->id);
    g_free (send);
}

/* Taking from the pending table makes response/cancel/send-error races
 * complete exactly once. The client supplies a take hook for initialize's
 * additional owner. Cancellation executes on the task's owning context. */
static gboolean
mcp_pending_send_cancelled (GCancellable *cancellable, gpointer data)
{
    McpPendingSend *send = data;
    g_autoptr(McpSession) session = g_weak_ref_get (&send->session);
    (void) cancellable;
    if (session != NULL)
    {
        g_autoptr(GTask) task = send->take (session, send->id);
        if (task != NULL)
            g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                     "MCP request cancelled");
    }
    return G_SOURCE_REMOVE;
}

static void
mcp_pending_source_free (gpointer data)
{
    GSource *source = data;
    g_source_destroy (source);
    g_source_unref (source);
}

static void
mcp_pending_send_done (GObject *source, GAsyncResult *result, gpointer data)
{
    McpPendingSend *send = data;
    g_autoptr(GError) error = NULL;
    if (!mcp_transport_send_message_finish (MCP_TRANSPORT (source), result, &error))
    {
        g_autoptr(McpSession) session = g_weak_ref_get (&send->session);
        if (session != NULL)
        {
            g_autoptr(GTask) task = send->take (session, send->id);
            if (task != NULL)
                g_task_return_error (task, g_steal_pointer (&error));
        }
    }
    mcp_pending_send_unref (send);
}

/* The transport write itself is not cancelled midway through an NDJSON frame.
 * Local cancellation retires the request immediately; late replies are ignored.
 * No cancellation source owns a strong session/task reference. */
static void
mcp_send_tracked_request (McpSession *session, McpTransport *transport,
                          McpRequest *request, GTask *task, McpTakeRequest take)
{
    McpPendingSend *send = g_new0 (McpPendingSend, 1);
    g_autoptr(JsonNode) node = mcp_message_to_json (MCP_MESSAGE (request));
    GCancellable *cancellable = g_task_get_cancellable (task);

    send->refs = 1;
    send->id = g_strdup (mcp_request_get_id (request));
    send->take = take;
    g_weak_ref_init (&send->session, session);
    mcp_session_add_pending_request (session, send->id, task);
    if (cancellable != NULL)
    {
        GSource *source = g_cancellable_source_new (cancellable);
        g_atomic_int_inc (&send->refs);
        g_source_set_callback (source, G_SOURCE_FUNC (mcp_pending_send_cancelled),
                               send, mcp_pending_send_unref);
        g_source_attach (source, g_task_get_context (task));
        g_object_set_data_full (G_OBJECT (task), "mcp-request-cancellation",
                                source, mcp_pending_source_free);
    }
    mcp_transport_send_message_async (transport, node, NULL,
                                      mcp_pending_send_done, send);
}

#endif
