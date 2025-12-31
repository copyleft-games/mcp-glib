/*
 * mcp-client.c - MCP Client implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp-client.h"
#include "mcp-message.h"
#include "mcp-error.h"
#include "mcp-version.h"
#include "mcp-task.h"
#undef MCP_COMPILATION

/**
 * SECTION:mcp-client
 * @title: McpClient
 * @short_description: MCP client implementation
 *
 * #McpClient provides a complete MCP client implementation that can
 * connect to MCP servers, list and call tools, read resources, and get prompts.
 */

struct _McpClient
{
    McpSession parent_instance;

    /* Capabilities */
    McpClientCapabilities *capabilities;
    McpServerCapabilities *server_capabilities;

    /* Server info */
    gchar *server_instructions;

    /* Transport */
    McpTransport *transport;
    gulong        message_handler_id;
    gulong        state_handler_id;
    gulong        error_handler_id;

    /* Pending connect task */
    GTask *connect_task;

    /* Roots management */
    GList *roots;  /* List of McpRoot* */
};

G_DEFINE_TYPE (McpClient, mcp_client, MCP_TYPE_SESSION)

enum
{
    PROP_0,
    PROP_CAPABILITIES,
    PROP_SERVER_CAPABILITIES,
    PROP_SERVER_INSTRUCTIONS,
    PROP_TRANSPORT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

enum
{
    SIGNAL_TOOLS_CHANGED,
    SIGNAL_RESOURCES_CHANGED,
    SIGNAL_PROMPTS_CHANGED,
    SIGNAL_RESOURCE_UPDATED,
    SIGNAL_LOG_MESSAGE,
    SIGNAL_SAMPLING_REQUESTED,
    SIGNAL_ROOTS_LIST_REQUESTED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

/* Forward declarations */
static void on_message_received (McpTransport *transport,
                                 JsonNode     *message,
                                 gpointer      user_data);
static void on_state_changed    (McpTransport      *transport,
                                 McpTransportState  old_state,
                                 McpTransportState  new_state,
                                 gpointer           user_data);
static void on_transport_error  (McpTransport *transport,
                                 GError       *error,
                                 gpointer      user_data);

static void handle_response     (McpClient   *self,
                                 McpResponse *response);
static void handle_error        (McpClient        *self,
                                 McpErrorResponse *error);
static void handle_notification (McpClient       *self,
                                 McpNotification *notification);
static void handle_request      (McpClient  *self,
                                 McpRequest *request);

static void
mcp_client_dispose (GObject *object)
{
    McpClient *self = MCP_CLIENT (object);

    if (self->transport != NULL)
    {
        if (self->message_handler_id > 0)
        {
            g_signal_handler_disconnect (self->transport, self->message_handler_id);
            self->message_handler_id = 0;
        }
        if (self->state_handler_id > 0)
        {
            g_signal_handler_disconnect (self->transport, self->state_handler_id);
            self->state_handler_id = 0;
        }
        if (self->error_handler_id > 0)
        {
            g_signal_handler_disconnect (self->transport, self->error_handler_id);
            self->error_handler_id = 0;
        }
        g_clear_object (&self->transport);
    }

    g_clear_object (&self->capabilities);
    g_clear_object (&self->server_capabilities);
    g_clear_object (&self->connect_task);

    G_OBJECT_CLASS (mcp_client_parent_class)->dispose (object);
}

static void
mcp_client_finalize (GObject *object)
{
    McpClient *self = MCP_CLIENT (object);

    g_free (self->server_instructions);
    g_list_free_full (self->roots, (GDestroyNotify)mcp_root_unref);

    G_OBJECT_CLASS (mcp_client_parent_class)->finalize (object);
}

static void
mcp_client_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    McpClient *self = MCP_CLIENT (object);

    switch (prop_id)
    {
        case PROP_CAPABILITIES:
            g_value_set_object (value, self->capabilities);
            break;
        case PROP_SERVER_CAPABILITIES:
            g_value_set_object (value, self->server_capabilities);
            break;
        case PROP_SERVER_INSTRUCTIONS:
            g_value_set_string (value, self->server_instructions);
            break;
        case PROP_TRANSPORT:
            g_value_set_object (value, self->transport);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_client_class_init (McpClientClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = mcp_client_dispose;
    object_class->finalize = mcp_client_finalize;
    object_class->get_property = mcp_client_get_property;

    properties[PROP_CAPABILITIES] =
        g_param_spec_object ("capabilities",
                             "Capabilities",
                             "The client capabilities",
                             MCP_TYPE_CLIENT_CAPABILITIES,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SERVER_CAPABILITIES] =
        g_param_spec_object ("server-capabilities",
                             "Server Capabilities",
                             "The connected server's capabilities",
                             MCP_TYPE_SERVER_CAPABILITIES,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SERVER_INSTRUCTIONS] =
        g_param_spec_string ("server-instructions",
                             "Server Instructions",
                             "The server's instructions for the LLM",
                             NULL,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_TRANSPORT] =
        g_param_spec_object ("transport",
                             "Transport",
                             "The transport for communication",
                             MCP_TYPE_TRANSPORT,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    /**
     * McpClient::tools-changed:
     * @self: the #McpClient
     *
     * Emitted when the server's tool list has changed.
     */
    signals[SIGNAL_TOOLS_CHANGED] =
        g_signal_new ("tools-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 0);

    /**
     * McpClient::resources-changed:
     * @self: the #McpClient
     *
     * Emitted when the server's resource list has changed.
     */
    signals[SIGNAL_RESOURCES_CHANGED] =
        g_signal_new ("resources-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 0);

    /**
     * McpClient::prompts-changed:
     * @self: the #McpClient
     *
     * Emitted when the server's prompt list has changed.
     */
    signals[SIGNAL_PROMPTS_CHANGED] =
        g_signal_new ("prompts-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 0);

    /**
     * McpClient::resource-updated:
     * @self: the #McpClient
     * @uri: the resource URI
     *
     * Emitted when a subscribed resource has been updated.
     */
    signals[SIGNAL_RESOURCE_UPDATED] =
        g_signal_new ("resource-updated",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 1,
                      G_TYPE_STRING);

    /**
     * McpClient::log-message:
     * @self: the #McpClient
     * @level: the log level string
     * @logger: (nullable): the logger name
     * @data: the log data as a #JsonNode
     *
     * Emitted when the server sends a log message.
     */
    signals[SIGNAL_LOG_MESSAGE] =
        g_signal_new ("log-message",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 3,
                      G_TYPE_STRING, G_TYPE_STRING, JSON_TYPE_NODE);

    /**
     * McpClient::sampling-requested:
     * @self: the #McpClient
     * @request_id: the JSON-RPC request ID
     * @messages: (element-type McpSamplingMessage): the messages list
     * @model_preferences: (nullable): the #McpModelPreferences
     * @system_prompt: (nullable): the system prompt
     * @max_tokens: the maximum tokens (-1 if not specified)
     *
     * Emitted when the server requests LLM sampling via sampling/createMessage.
     * The signal handler should call mcp_client_respond_sampling() or
     * mcp_client_reject_sampling() to respond.
     */
    signals[SIGNAL_SAMPLING_REQUESTED] =
        g_signal_new ("sampling-requested",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 5,
                      G_TYPE_STRING,      /* request_id */
                      G_TYPE_POINTER,     /* messages (GList*) */
                      G_TYPE_POINTER,     /* model_preferences */
                      G_TYPE_STRING,      /* system_prompt */
                      G_TYPE_INT64);      /* max_tokens */

    /**
     * McpClient::roots-list-requested:
     * @self: the #McpClient
     * @request_id: the JSON-RPC request ID
     *
     * Emitted when the server requests the list of roots via roots/list.
     * The client will automatically respond with its current roots list.
     */
    signals[SIGNAL_ROOTS_LIST_REQUESTED] =
        g_signal_new ("roots-list-requested",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 1,
                      G_TYPE_STRING);     /* request_id */
}

static void
mcp_client_init (McpClient *self)
{
    self->capabilities = mcp_client_capabilities_new ();
}

/**
 * mcp_client_new:
 * @name: the client name
 * @version: the client version
 *
 * Creates a new MCP client.
 *
 * Returns: (transfer full): a new #McpClient
 */
McpClient *
mcp_client_new (const gchar *name,
                const gchar *version)
{
    McpClient *self;
    McpImplementation *impl;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (version != NULL, NULL);

    self = g_object_new (MCP_TYPE_CLIENT, NULL);

    impl = mcp_implementation_new (name, version);
    mcp_session_set_local_implementation (MCP_SESSION (self), impl);
    g_object_unref (impl);

    return self;
}

/* Property getters */

McpClientCapabilities *
mcp_client_get_capabilities (McpClient *self)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    return self->capabilities;
}

McpServerCapabilities *
mcp_client_get_server_capabilities (McpClient *self)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    return self->server_capabilities;
}

const gchar *
mcp_client_get_server_instructions (McpClient *self)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    return self->server_instructions;
}

/* Transport management */

void
mcp_client_set_transport (McpClient    *self,
                          McpTransport *transport)
{
    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (MCP_IS_TRANSPORT (transport));

    if (self->transport != NULL)
    {
        if (self->message_handler_id > 0)
        {
            g_signal_handler_disconnect (self->transport, self->message_handler_id);
            self->message_handler_id = 0;
        }
        if (self->state_handler_id > 0)
        {
            g_signal_handler_disconnect (self->transport, self->state_handler_id);
            self->state_handler_id = 0;
        }
        if (self->error_handler_id > 0)
        {
            g_signal_handler_disconnect (self->transport, self->error_handler_id);
            self->error_handler_id = 0;
        }
        g_clear_object (&self->transport);
    }

    self->transport = g_object_ref (transport);

    self->message_handler_id = g_signal_connect (transport, "message-received",
                                                  G_CALLBACK (on_message_received),
                                                  self);
    self->state_handler_id = g_signal_connect (transport, "state-changed",
                                                G_CALLBACK (on_state_changed),
                                                self);
    self->error_handler_id = g_signal_connect (transport, "error",
                                                G_CALLBACK (on_transport_error),
                                                self);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TRANSPORT]);
}

McpTransport *
mcp_client_get_transport (McpClient *self)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    return self->transport;
}

/* Helper to send a request and track it */
static void
send_request (McpClient   *self,
              McpRequest  *request,
              GTask       *task)
{
    g_autoptr(JsonNode) node = NULL;
    const gchar *id;

    id = mcp_request_get_id (request);
    mcp_session_add_pending_request (MCP_SESSION (self), id, task);

    node = mcp_message_to_json (MCP_MESSAGE (request));
    mcp_transport_send_message_async (self->transport, node, NULL, NULL, NULL);
}

/* Connection */

static void
send_initialize_request (McpClient *self)
{
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autofree gchar *id = NULL;
    McpImplementation *impl;

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("initialize", id);

    impl = mcp_session_get_local_implementation (MCP_SESSION (self));

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "protocolVersion");
    json_builder_add_string_value (builder, MCP_PROTOCOL_VERSION);

    json_builder_set_member_name (builder, "capabilities");
    {
        g_autoptr(JsonNode) caps = mcp_client_capabilities_to_json (self->capabilities);
        json_builder_add_value (builder, g_steal_pointer (&caps));
    }

    json_builder_set_member_name (builder, "clientInfo");
    {
        g_autoptr(JsonNode) info = mcp_implementation_to_json (impl);
        json_builder_add_value (builder, g_steal_pointer (&info));
    }

    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    mcp_request_set_params (request, g_steal_pointer (&params));

    send_request (self, request, self->connect_task);
}

static void
transport_connect_cb (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
    McpClient *self = MCP_CLIENT (user_data);
    GError *error = NULL;

    if (!mcp_transport_connect_finish (MCP_TRANSPORT (source), result, &error))
    {
        if (self->connect_task != NULL)
        {
            g_task_return_error (self->connect_task, error);
            g_clear_object (&self->connect_task);
        }
        else
        {
            g_error_free (error);
        }
        return;
    }

    /* Transport connected, now send initialize request */
    mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_INITIALIZING);
    send_initialize_request (self);
}

void
mcp_client_connect_async (McpClient           *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
    g_return_if_fail (MCP_IS_CLIENT (self));

    if (self->connect_task != NULL)
    {
        g_task_report_new_error (self, callback, user_data, mcp_client_connect_async,
                                 MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client already connecting");
        return;
    }

    if (self->transport == NULL)
    {
        g_task_report_new_error (self, callback, user_data, mcp_client_connect_async,
                                 MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "No transport set");
        return;
    }

    self->connect_task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (self->connect_task, mcp_client_connect_async);

    mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_CONNECTING);
    mcp_transport_connect_async (self->transport, cancellable, transport_connect_cb, self);
}

gboolean
mcp_client_connect_finish (McpClient     *self,
                           GAsyncResult  *result,
                           GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
transport_disconnect_cb (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
    GTask *task = G_TASK (user_data);
    GError *error = NULL;

    if (!mcp_transport_disconnect_finish (MCP_TRANSPORT (source), result, &error))
    {
        g_task_return_error (task, error);
    }
    else
    {
        g_task_return_boolean (task, TRUE);
    }

    g_object_unref (task);
}

void
mcp_client_disconnect_async (McpClient           *self,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    GTask *task;

    g_return_if_fail (MCP_IS_CLIENT (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_disconnect_async);

    if (self->transport == NULL)
    {
        g_task_return_boolean (task, TRUE);
        g_object_unref (task);
        return;
    }

    mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_CLOSING);
    mcp_transport_disconnect_async (self->transport, cancellable, transport_disconnect_cb, task);
}

gboolean
mcp_client_disconnect_finish (McpClient     *self,
                              GAsyncResult  *result,
                              GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

/* Tool operations */

void
mcp_client_list_tools_async (McpClient           *self,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_list_tools_async);
    g_task_set_task_data (task, GINT_TO_POINTER (1), NULL); /* Mark as tools/list */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("tools/list", id);

    send_request (self, request, task);
    g_object_unref (task);
}

GList *
mcp_client_list_tools_finish (McpClient     *self,
                              GAsyncResult  *result,
                              GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

void
mcp_client_call_tool_async (McpClient           *self,
                            const gchar         *name,
                            JsonObject          *arguments,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (name != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_call_tool_async);
    g_task_set_task_data (task, GINT_TO_POINTER (2), NULL); /* Mark as tools/call */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("tools/call", id);

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "name");
    json_builder_add_string_value (builder, name);
    if (arguments != NULL)
    {
        json_builder_set_member_name (builder, "arguments");
        json_builder_add_value (builder, json_node_init_object (json_node_new (JSON_NODE_OBJECT),
                                                                 arguments));
    }
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    mcp_request_set_params (request, g_steal_pointer (&params));
    send_request (self, request, task);
    g_object_unref (task);
}

McpToolResult *
mcp_client_call_tool_finish (McpClient     *self,
                             GAsyncResult  *result,
                             GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

/* Resource operations */

void
mcp_client_list_resources_async (McpClient           *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_list_resources_async);
    g_task_set_task_data (task, GINT_TO_POINTER (3), NULL); /* Mark as resources/list */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("resources/list", id);

    send_request (self, request, task);
    g_object_unref (task);
}

GList *
mcp_client_list_resources_finish (McpClient     *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

void
mcp_client_list_resource_templates_async (McpClient           *self,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_list_resource_templates_async);
    g_task_set_task_data (task, GINT_TO_POINTER (4), NULL); /* Mark as resources/templates/list */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("resources/templates/list", id);

    send_request (self, request, task);
    g_object_unref (task);
}

GList *
mcp_client_list_resource_templates_finish (McpClient     *self,
                                           GAsyncResult  *result,
                                           GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

void
mcp_client_read_resource_async (McpClient           *self,
                                const gchar         *uri,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (uri != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_read_resource_async);
    g_task_set_task_data (task, GINT_TO_POINTER (5), NULL); /* Mark as resources/read */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("resources/read", id);

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "uri");
    json_builder_add_string_value (builder, uri);
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    mcp_request_set_params (request, g_steal_pointer (&params));
    send_request (self, request, task);
    g_object_unref (task);
}

GList *
mcp_client_read_resource_finish (McpClient     *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

void
mcp_client_subscribe_resource_async (McpClient           *self,
                                     const gchar         *uri,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (uri != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_subscribe_resource_async);
    g_task_set_task_data (task, GINT_TO_POINTER (6), NULL); /* Mark as resources/subscribe */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("resources/subscribe", id);

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "uri");
    json_builder_add_string_value (builder, uri);
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    mcp_request_set_params (request, g_steal_pointer (&params));
    send_request (self, request, task);
    g_object_unref (task);
}

gboolean
mcp_client_subscribe_resource_finish (McpClient     *self,
                                      GAsyncResult  *result,
                                      GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

void
mcp_client_unsubscribe_resource_async (McpClient           *self,
                                       const gchar         *uri,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (uri != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_unsubscribe_resource_async);
    g_task_set_task_data (task, GINT_TO_POINTER (7), NULL); /* Mark as resources/unsubscribe */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("resources/unsubscribe", id);

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "uri");
    json_builder_add_string_value (builder, uri);
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    mcp_request_set_params (request, g_steal_pointer (&params));
    send_request (self, request, task);
    g_object_unref (task);
}

gboolean
mcp_client_unsubscribe_resource_finish (McpClient     *self,
                                        GAsyncResult  *result,
                                        GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

/* Prompt operations */

void
mcp_client_list_prompts_async (McpClient           *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_list_prompts_async);
    g_task_set_task_data (task, GINT_TO_POINTER (8), NULL); /* Mark as prompts/list */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("prompts/list", id);

    send_request (self, request, task);
    g_object_unref (task);
}

GList *
mcp_client_list_prompts_finish (McpClient     *self,
                                GAsyncResult  *result,
                                GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

void
mcp_client_get_prompt_async (McpClient           *self,
                             const gchar         *name,
                             GHashTable          *arguments,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (name != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_get_prompt_async);
    g_task_set_task_data (task, GINT_TO_POINTER (9), NULL); /* Mark as prompts/get */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("prompts/get", id);

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "name");
    json_builder_add_string_value (builder, name);

    if (arguments != NULL && g_hash_table_size (arguments) > 0)
    {
        GHashTableIter iter;
        gpointer key, value;

        json_builder_set_member_name (builder, "arguments");
        json_builder_begin_object (builder);
        g_hash_table_iter_init (&iter, arguments);
        while (g_hash_table_iter_next (&iter, &key, &value))
        {
            json_builder_set_member_name (builder, (const gchar *) key);
            json_builder_add_string_value (builder, (const gchar *) value);
        }
        json_builder_end_object (builder);
    }

    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    mcp_request_set_params (request, g_steal_pointer (&params));
    send_request (self, request, task);
    g_object_unref (task);
}

McpPromptResult *
mcp_client_get_prompt_finish (McpClient     *self,
                              GAsyncResult  *result,
                              GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

/* Ping */

void
mcp_client_ping_async (McpClient           *self,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_ping_async);
    g_task_set_task_data (task, GINT_TO_POINTER (10), NULL); /* Mark as ping */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("ping", id);

    send_request (self, request, task);
    g_object_unref (task);
}

gboolean
mcp_client_ping_finish (McpClient     *self,
                        GAsyncResult  *result,
                        GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

/* Transport callbacks */

/*
 * send_message_cb:
 *
 * Generic callback for send operations.
 */
static void
send_message_cb (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    GError *error = NULL;

    if (!mcp_transport_send_message_finish (MCP_TRANSPORT (source), result, &error))
    {
        g_warning ("Failed to send message: %s", error->message);
        g_error_free (error);
    }
}

/*
 * send_response:
 * @self: the client
 * @id: the request ID
 * @result_node: the result to send
 *
 * Sends a JSON-RPC response to the server.
 */
static void
send_response (McpClient *self,
               const gchar *id,
               JsonNode    *result_node)
{
    g_autoptr(McpResponse) response = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(JsonNode) result_owned = result_node;

    if (self->transport == NULL)
    {
        return;
    }

    response = mcp_response_new (id, g_steal_pointer (&result_owned));
    node = mcp_message_to_json (MCP_MESSAGE (response));

    mcp_transport_send_message_async (self->transport, node, NULL,
                                      send_message_cb, NULL);
}

/*
 * send_error_response:
 * @self: the client
 * @id: the request ID (can be NULL)
 * @code: the error code
 * @message: the error message
 * @data: (nullable): additional error data
 *
 * Sends a JSON-RPC error response to the server.
 */
static void
send_error_response (McpClient   *self,
                     const gchar *id,
                     gint         code,
                     const gchar *message,
                     JsonNode    *data)
{
    g_autoptr(McpErrorResponse) error_resp = NULL;
    g_autoptr(JsonNode) node = NULL;

    if (self->transport == NULL)
    {
        return;
    }

    error_resp = mcp_error_response_new (id, code, message);
    if (data != NULL)
    {
        mcp_error_response_set_data (error_resp, data);
    }
    node = mcp_message_to_json (MCP_MESSAGE (error_resp));

    mcp_transport_send_message_async (self->transport, node, NULL,
                                      send_message_cb, NULL);
}

static void
on_message_received (McpTransport *transport,
                     JsonNode     *message,
                     gpointer      user_data)
{
    McpClient *self = MCP_CLIENT (user_data);
    g_autoptr(McpMessage) msg = NULL;
    g_autoptr(GError) error = NULL;

    msg = mcp_message_new_from_json (message, &error);
    if (msg == NULL)
    {
        g_warning ("Failed to parse message: %s", error->message);
        return;
    }

    switch (mcp_message_get_message_type (msg))
    {
        case MCP_MESSAGE_TYPE_REQUEST:
            handle_request (self, MCP_REQUEST (msg));
            break;
        case MCP_MESSAGE_TYPE_RESPONSE:
            handle_response (self, MCP_RESPONSE (msg));
            break;
        case MCP_MESSAGE_TYPE_ERROR:
            handle_error (self, MCP_ERROR_RESPONSE (msg));
            break;
        case MCP_MESSAGE_TYPE_NOTIFICATION:
            handle_notification (self, MCP_NOTIFICATION (msg));
            break;
        default:
            g_warning ("Unexpected message type: %d",
                       mcp_message_get_message_type (msg));
            break;
    }
}

static void
on_state_changed (McpTransport      *transport,
                  McpTransportState  old_state,
                  McpTransportState  new_state,
                  gpointer           user_data)
{
    McpClient *self = MCP_CLIENT (user_data);

    if (new_state == MCP_TRANSPORT_STATE_DISCONNECTED)
    {
        mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_DISCONNECTED);
        mcp_session_cancel_all_pending_requests (MCP_SESSION (self), NULL);
    }
    else if (new_state == MCP_TRANSPORT_STATE_ERROR)
    {
        mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_ERROR);
        mcp_session_cancel_all_pending_requests (MCP_SESSION (self), NULL);
    }
}

static void
on_transport_error (McpTransport *transport,
                    GError       *error,
                    gpointer      user_data)
{
    McpClient *self = MCP_CLIENT (user_data);

    g_warning ("Transport error: %s", error->message);

    if (self->connect_task != NULL)
    {
        g_task_return_error (self->connect_task, g_error_copy (error));
        g_clear_object (&self->connect_task);
    }
}

/* Response handling */

/*
 * on_initialized_notification_sent:
 *
 * Callback called after the notifications/initialized message has been sent.
 * We complete the connect_task here to ensure the client doesn't try to send
 * another message while the output stream still has an outstanding write.
 */
static void
on_initialized_notification_sent (GObject      *source,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
    McpClient *self = MCP_CLIENT (user_data);
    g_autoptr(GError) error = NULL;

    if (!mcp_transport_send_message_finish (MCP_TRANSPORT (source), result, &error))
    {
        g_warning ("Failed to send initialized notification: %s", error->message);
    }

    /* Now that the send is complete, we can safely complete the connect task */
    if (self->connect_task != NULL)
    {
        g_task_return_boolean (self->connect_task, TRUE);
        g_clear_object (&self->connect_task);
    }
}

static void
handle_initialize_response (McpClient   *self,
                            McpResponse *response)
{
    JsonNode *result;
    JsonObject *obj;
    g_autoptr(GError) error = NULL;

    result = mcp_response_get_result (response);
    if (!JSON_NODE_HOLDS_OBJECT (result))
    {
        if (self->connect_task != NULL)
        {
            g_task_return_new_error (self->connect_task, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                                     "Invalid initialize response");
            g_clear_object (&self->connect_task);
        }
        return;
    }

    obj = json_node_get_object (result);

    /* Parse server capabilities */
    if (json_object_has_member (obj, "capabilities"))
    {
        JsonNode *caps_node = json_object_get_member (obj, "capabilities");
        self->server_capabilities = mcp_server_capabilities_new_from_json (caps_node, &error);
        if (self->server_capabilities == NULL)
        {
            g_warning ("Failed to parse server capabilities: %s", error->message);
            self->server_capabilities = mcp_server_capabilities_new ();
        }
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SERVER_CAPABILITIES]);
    }

    /* Parse server info */
    if (json_object_has_member (obj, "serverInfo"))
    {
        JsonNode *info_node = json_object_get_member (obj, "serverInfo");
        McpImplementation *server_impl;

        server_impl = mcp_implementation_new_from_json (info_node, NULL);
        if (server_impl != NULL)
        {
            mcp_session_set_remote_implementation (MCP_SESSION (self), server_impl);
            g_object_unref (server_impl);
        }
    }

    /* Parse instructions */
    if (json_object_has_member (obj, "instructions"))
    {
        self->server_instructions = g_strdup (json_object_get_string_member (obj, "instructions"));
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SERVER_INSTRUCTIONS]);
    }

    /* Parse protocol version */
    if (json_object_has_member (obj, "protocolVersion"))
    {
        const gchar *version = json_object_get_string_member (obj, "protocolVersion");
        mcp_session_set_protocol_version (MCP_SESSION (self), version);
    }

    /* Now ready */
    mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_READY);

    /*
     * Send initialized notification. The connect_task will be completed
     * in the callback after the send finishes, to avoid the client trying
     * to send another message while this one is still in flight.
     */
    {
        g_autoptr(McpNotification) notif = NULL;
        g_autoptr(JsonNode) node = NULL;

        notif = mcp_notification_new ("notifications/initialized");
        node = mcp_message_to_json (MCP_MESSAGE (notif));
        mcp_transport_send_message_async (self->transport, node, NULL,
                                          on_initialized_notification_sent, self);
    }
}

static GList *
parse_tools_response (JsonNode *result)
{
    GList *list = NULL;
    JsonObject *obj;
    JsonArray *tools_arr;
    guint i, len;

    if (!JSON_NODE_HOLDS_OBJECT (result))
    {
        return NULL;
    }

    obj = json_node_get_object (result);
    if (!json_object_has_member (obj, "tools"))
    {
        return NULL;
    }

    tools_arr = json_object_get_array_member (obj, "tools");
    len = json_array_get_length (tools_arr);

    for (i = 0; i < len; i++)
    {
        JsonNode *tool_node = json_array_get_element (tools_arr, i);
        McpTool *tool = mcp_tool_new_from_json (tool_node, NULL);
        if (tool != NULL)
        {
            list = g_list_prepend (list, tool);
        }
    }

    return g_list_reverse (list);
}

static McpToolResult *
parse_tool_call_response (JsonNode *result)
{
    JsonObject *obj;
    JsonArray *content;
    gboolean is_error = FALSE;
    McpToolResult *tool_result;
    guint i, len;

    if (!JSON_NODE_HOLDS_OBJECT (result))
    {
        return NULL;
    }

    obj = json_node_get_object (result);

    if (json_object_has_member (obj, "isError"))
    {
        is_error = json_object_get_boolean_member (obj, "isError");
    }

    tool_result = mcp_tool_result_new (is_error);

    if (json_object_has_member (obj, "content"))
    {
        content = json_object_get_array_member (obj, "content");
        len = json_array_get_length (content);

        for (i = 0; i < len; i++)
        {
            JsonObject *item = json_array_get_object_element (content, i);
            const gchar *type = json_object_get_string_member (item, "type");

            if (g_strcmp0 (type, "text") == 0)
            {
                const gchar *text = json_object_get_string_member (item, "text");
                mcp_tool_result_add_text (tool_result, text);
            }
            else if (g_strcmp0 (type, "image") == 0)
            {
                const gchar *data = json_object_get_string_member (item, "data");
                const gchar *mime_type = json_object_get_string_member (item, "mimeType");
                mcp_tool_result_add_image (tool_result, data, mime_type);
            }
        }
    }

    return tool_result;
}

static GList *
parse_resources_response (JsonNode *result)
{
    GList *list = NULL;
    JsonObject *obj;
    JsonArray *res_arr;
    guint i, len;

    if (!JSON_NODE_HOLDS_OBJECT (result))
    {
        return NULL;
    }

    obj = json_node_get_object (result);
    if (!json_object_has_member (obj, "resources"))
    {
        return NULL;
    }

    res_arr = json_object_get_array_member (obj, "resources");
    len = json_array_get_length (res_arr);

    for (i = 0; i < len; i++)
    {
        JsonNode *res_node = json_array_get_element (res_arr, i);
        McpResource *resource = mcp_resource_new_from_json (res_node, NULL);
        if (resource != NULL)
        {
            list = g_list_prepend (list, resource);
        }
    }

    return g_list_reverse (list);
}

static GList *
parse_resource_templates_response (JsonNode *result)
{
    GList *list = NULL;
    JsonObject *obj;
    JsonArray *templ_arr;
    guint i, len;

    if (!JSON_NODE_HOLDS_OBJECT (result))
    {
        return NULL;
    }

    obj = json_node_get_object (result);
    if (!json_object_has_member (obj, "resourceTemplates"))
    {
        return NULL;
    }

    templ_arr = json_object_get_array_member (obj, "resourceTemplates");
    len = json_array_get_length (templ_arr);

    for (i = 0; i < len; i++)
    {
        JsonNode *templ_node = json_array_get_element (templ_arr, i);
        McpResourceTemplate *templ = mcp_resource_template_new_from_json (templ_node, NULL);
        if (templ != NULL)
        {
            list = g_list_prepend (list, templ);
        }
    }

    return g_list_reverse (list);
}

static GList *
parse_resource_read_response (JsonNode *result)
{
    GList *list = NULL;
    JsonObject *obj;
    JsonArray *contents_arr;
    guint i, len;

    if (!JSON_NODE_HOLDS_OBJECT (result))
    {
        return NULL;
    }

    obj = json_node_get_object (result);
    if (!json_object_has_member (obj, "contents"))
    {
        return NULL;
    }

    contents_arr = json_object_get_array_member (obj, "contents");
    len = json_array_get_length (contents_arr);

    for (i = 0; i < len; i++)
    {
        JsonObject *item = json_array_get_object_element (contents_arr, i);
        const gchar *uri = json_object_get_string_member (item, "uri");
        const gchar *mime_type = NULL;
        McpResourceContents *contents = NULL;

        if (json_object_has_member (item, "mimeType"))
        {
            mime_type = json_object_get_string_member (item, "mimeType");
        }

        if (json_object_has_member (item, "text"))
        {
            const gchar *text = json_object_get_string_member (item, "text");
            contents = mcp_resource_contents_new_text (uri, text, mime_type);
        }
        else if (json_object_has_member (item, "blob"))
        {
            const gchar *blob = json_object_get_string_member (item, "blob");
            contents = mcp_resource_contents_new_blob (uri, blob, mime_type);
        }

        if (contents != NULL)
        {
            list = g_list_prepend (list, contents);
        }
    }

    return g_list_reverse (list);
}

static GList *
parse_prompts_response (JsonNode *result)
{
    GList *list = NULL;
    JsonObject *obj;
    JsonArray *prompts_arr;
    guint i, len;

    if (!JSON_NODE_HOLDS_OBJECT (result))
    {
        return NULL;
    }

    obj = json_node_get_object (result);
    if (!json_object_has_member (obj, "prompts"))
    {
        return NULL;
    }

    prompts_arr = json_object_get_array_member (obj, "prompts");
    len = json_array_get_length (prompts_arr);

    for (i = 0; i < len; i++)
    {
        JsonNode *prompt_node = json_array_get_element (prompts_arr, i);
        McpPrompt *prompt = mcp_prompt_new_from_json (prompt_node, NULL);
        if (prompt != NULL)
        {
            list = g_list_prepend (list, prompt);
        }
    }

    return g_list_reverse (list);
}

static McpPromptResult *
parse_prompt_get_response (JsonNode *result)
{
    JsonObject *obj;
    const gchar *description = NULL;
    McpPromptResult *prompt_result;
    JsonArray *messages;
    guint i, len;

    if (!JSON_NODE_HOLDS_OBJECT (result))
    {
        return NULL;
    }

    obj = json_node_get_object (result);

    if (json_object_has_member (obj, "description"))
    {
        description = json_object_get_string_member (obj, "description");
    }

    prompt_result = mcp_prompt_result_new (description);

    if (json_object_has_member (obj, "messages"))
    {
        messages = json_object_get_array_member (obj, "messages");
        len = json_array_get_length (messages);

        for (i = 0; i < len; i++)
        {
            JsonObject *msg_obj = json_array_get_object_element (messages, i);
            const gchar *role_str = json_object_get_string_member (msg_obj, "role");
            McpRole role = mcp_role_from_string (role_str);
            McpPromptMessage *msg = mcp_prompt_message_new (role);
            JsonArray *content;
            guint j, content_len;

            if (json_object_has_member (msg_obj, "content"))
            {
                content = json_object_get_array_member (msg_obj, "content");
                content_len = json_array_get_length (content);

                for (j = 0; j < content_len; j++)
                {
                    JsonObject *content_obj = json_array_get_object_element (content, j);
                    const gchar *type = json_object_get_string_member (content_obj, "type");

                    if (g_strcmp0 (type, "text") == 0)
                    {
                        const gchar *text = json_object_get_string_member (content_obj, "text");
                        mcp_prompt_message_add_text (msg, text);
                    }
                    else if (g_strcmp0 (type, "image") == 0)
                    {
                        const gchar *data = json_object_get_string_member (content_obj, "data");
                        const gchar *mime_type = json_object_get_string_member (content_obj, "mimeType");
                        mcp_prompt_message_add_image (msg, data, mime_type);
                    }
                }
            }

            mcp_prompt_result_add_message (prompt_result, msg);
            mcp_prompt_message_unref (msg);
        }
    }

    return prompt_result;
}

static void
handle_response (McpClient   *self,
                 McpResponse *response)
{
    const gchar *id;
    GTask *task;

    id = mcp_response_get_id (response);
    task = mcp_session_take_pending_request (MCP_SESSION (self), id);

    if (task == NULL)
    {
        g_warning ("Received response for unknown request: %s", id);
        return;
    }

    /* Check if this is the connect task (initialize response) */
    if (g_task_get_source_tag (task) == mcp_client_connect_async)
    {
        handle_initialize_response (self, response);
        g_object_unref (task);
        return;
    }

    /* Handle based on request type (stored as task data) */
    {
        gint request_type = GPOINTER_TO_INT (g_task_get_task_data (task));
        JsonNode *result = mcp_response_get_result (response);

        switch (request_type)
        {
            case 1: /* tools/list */
                {
                    GList *tools = parse_tools_response (result);
                    g_task_return_pointer (task, tools, NULL);
                }
                break;

            case 2: /* tools/call */
                {
                    McpToolResult *tool_result = parse_tool_call_response (result);
                    g_task_return_pointer (task, tool_result, (GDestroyNotify) mcp_tool_result_unref);
                }
                break;

            case 3: /* resources/list */
                {
                    GList *resources = parse_resources_response (result);
                    g_task_return_pointer (task, resources, NULL);
                }
                break;

            case 4: /* resources/templates/list */
                {
                    GList *templates = parse_resource_templates_response (result);
                    g_task_return_pointer (task, templates, NULL);
                }
                break;

            case 5: /* resources/read */
                {
                    GList *contents = parse_resource_read_response (result);
                    g_task_return_pointer (task, contents, NULL);
                }
                break;

            case 6: /* resources/subscribe */
            case 7: /* resources/unsubscribe */
                g_task_return_boolean (task, TRUE);
                break;

            case 8: /* prompts/list */
                {
                    GList *prompts = parse_prompts_response (result);
                    g_task_return_pointer (task, prompts, NULL);
                }
                break;

            case 9: /* prompts/get */
                {
                    McpPromptResult *prompt_result = parse_prompt_get_response (result);
                    g_task_return_pointer (task, prompt_result, (GDestroyNotify) mcp_prompt_result_unref);
                }
                break;

            case 10: /* ping */
                g_task_return_boolean (task, TRUE);
                break;

            case 11: /* tasks/get */
                {
                    McpTask *mcp_task = mcp_task_new_from_json (result, NULL);
                    g_task_return_pointer (task, mcp_task, g_object_unref);
                }
                break;

            case 12: /* tasks/result */
                {
                    McpToolResult *tool_result = parse_tool_call_response (result);
                    g_task_return_pointer (task, tool_result, (GDestroyNotify) mcp_tool_result_unref);
                }
                break;

            case 13: /* tasks/cancel */
                g_task_return_boolean (task, TRUE);
                break;

            case 14: /* tasks/list */
                {
                    GList *list = NULL;
                    if (JSON_NODE_HOLDS_OBJECT (result))
                    {
                        JsonObject *obj = json_node_get_object (result);
                        if (json_object_has_member (obj, "tasks"))
                        {
                            JsonArray *tasks_arr = json_object_get_array_member (obj, "tasks");
                            guint i, len = json_array_get_length (tasks_arr);
                            for (i = 0; i < len; i++)
                            {
                                JsonNode *task_node = json_array_get_element (tasks_arr, i);
                                McpTask *mcp_task = mcp_task_new_from_json (task_node, NULL);
                                if (mcp_task != NULL)
                                {
                                    list = g_list_prepend (list, mcp_task);
                                }
                            }
                            list = g_list_reverse (list);
                        }
                    }
                    g_task_return_pointer (task, list, NULL);
                }
                break;

            default:
                g_task_return_boolean (task, TRUE);
                break;
        }
    }

    g_object_unref (task);
}

static void
handle_error (McpClient        *self,
              McpErrorResponse *error)
{
    const gchar *id;
    GTask *task;
    GError *gerror;

    id = mcp_error_response_get_id (error);
    if (id == NULL)
    {
        g_warning ("Received error response without ID");
        return;
    }

    task = mcp_session_take_pending_request (MCP_SESSION (self), id);
    if (task == NULL)
    {
        g_warning ("Received error response for unknown request: %s", id);
        return;
    }

    gerror = mcp_error_response_to_gerror (error);
    g_task_return_error (task, gerror);
    g_object_unref (task);
}

static void
handle_notification (McpClient       *self,
                     McpNotification *notification)
{
    const gchar *method;
    JsonNode *params;

    method = mcp_notification_get_method (notification);
    params = mcp_notification_get_params (notification);

    if (g_strcmp0 (method, "notifications/tools/list_changed") == 0)
    {
        g_signal_emit (self, signals[SIGNAL_TOOLS_CHANGED], 0);
    }
    else if (g_strcmp0 (method, "notifications/resources/list_changed") == 0)
    {
        g_signal_emit (self, signals[SIGNAL_RESOURCES_CHANGED], 0);
    }
    else if (g_strcmp0 (method, "notifications/prompts/list_changed") == 0)
    {
        g_signal_emit (self, signals[SIGNAL_PROMPTS_CHANGED], 0);
    }
    else if (g_strcmp0 (method, "notifications/resources/updated") == 0)
    {
        if (params != NULL && JSON_NODE_HOLDS_OBJECT (params))
        {
            JsonObject *obj = json_node_get_object (params);
            if (json_object_has_member (obj, "uri"))
            {
                const gchar *uri = json_object_get_string_member (obj, "uri");
                g_signal_emit (self, signals[SIGNAL_RESOURCE_UPDATED], 0, uri);
            }
        }
    }
    else if (g_strcmp0 (method, "notifications/message") == 0)
    {
        if (params != NULL && JSON_NODE_HOLDS_OBJECT (params))
        {
            JsonObject *obj = json_node_get_object (params);
            const gchar *level = json_object_get_string_member (obj, "level");
            const gchar *logger = NULL;
            JsonNode *data = NULL;

            if (json_object_has_member (obj, "logger"))
            {
                logger = json_object_get_string_member (obj, "logger");
            }
            if (json_object_has_member (obj, "data"))
            {
                data = json_object_get_member (obj, "data");
            }

            g_signal_emit (self, signals[SIGNAL_LOG_MESSAGE], 0, level, logger, data);
        }
    }
    else if (g_strcmp0 (method, "notifications/tasks/status") == 0)
    {
        /* Task status notification - could emit a signal here if needed */
        /* For now, just acknowledge it */
    }
}

/*
 * handle_request:
 * @self: the client
 * @request: the incoming request from server
 *
 * Handles server-initiated requests (role reversal for sampling/roots).
 */
static void
handle_request (McpClient  *self,
                McpRequest *request)
{
    const gchar *method;
    const gchar *id;
    JsonNode *params;

    method = mcp_request_get_method (request);
    id = mcp_request_get_id (request);
    params = mcp_request_get_params (request);

    if (g_strcmp0 (method, "sampling/createMessage") == 0)
    {
        /*
         * Server is requesting LLM sampling from the client.
         * Parse the request and emit the sampling-requested signal.
         */
        JsonObject *obj;
        JsonArray *messages_arr;
        GList *messages = NULL;
        McpModelPreferences *model_prefs = NULL;
        const gchar *system_prompt = NULL;
        gint64 max_tokens = -1;
        guint i;
        guint len;

        if (params == NULL || !JSON_NODE_HOLDS_OBJECT (params))
        {
            send_error_response (self, id, MCP_ERROR_INVALID_PARAMS,
                                 "Invalid sampling request params", NULL);
            return;
        }

        obj = json_node_get_object (params);

        /* Parse messages */
        if (!json_object_has_member (obj, "messages"))
        {
            send_error_response (self, id, MCP_ERROR_INVALID_PARAMS,
                                 "Missing 'messages' field", NULL);
            return;
        }

        messages_arr = json_object_get_array_member (obj, "messages");
        len = json_array_get_length (messages_arr);
        for (i = 0; i < len; i++)
        {
            JsonNode *msg_node = json_array_get_element (messages_arr, i);
            g_autoptr(GError) error = NULL;
            McpSamplingMessage *msg;

            msg = mcp_sampling_message_new_from_json (msg_node, &error);
            if (msg != NULL)
            {
                messages = g_list_append (messages, msg);
            }
            else
            {
                g_warning ("Failed to parse sampling message: %s", error->message);
            }
        }

        /* Parse optional model preferences */
        if (json_object_has_member (obj, "modelPreferences"))
        {
            JsonNode *prefs_node = json_object_get_member (obj, "modelPreferences");
            model_prefs = mcp_model_preferences_new_from_json (prefs_node, NULL);
        }

        /* Parse optional system prompt */
        if (json_object_has_member (obj, "systemPrompt"))
        {
            system_prompt = json_object_get_string_member (obj, "systemPrompt");
        }

        /* Parse optional max tokens */
        if (json_object_has_member (obj, "maxTokens"))
        {
            max_tokens = json_object_get_int_member (obj, "maxTokens");
        }

        /* Emit signal for application to handle */
        g_signal_emit (self, signals[SIGNAL_SAMPLING_REQUESTED], 0,
                       id, messages, model_prefs, system_prompt, max_tokens);

        /* Clean up (application should have taken references if needed) */
        g_list_free_full (messages, (GDestroyNotify)mcp_sampling_message_unref);
        if (model_prefs != NULL)
        {
            mcp_model_preferences_unref (model_prefs);
        }
    }
    else if (g_strcmp0 (method, "roots/list") == 0)
    {
        /*
         * Server is requesting the list of roots from the client.
         * Respond with the current roots list.
         */
        JsonBuilder *builder;
        JsonNode *result;
        GList *l;

        /* Emit signal so application knows the request was made */
        g_signal_emit (self, signals[SIGNAL_ROOTS_LIST_REQUESTED], 0, id);

        /* Build response */
        builder = json_builder_new ();
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "roots");
        json_builder_begin_array (builder);

        for (l = self->roots; l != NULL; l = l->next)
        {
            McpRoot *root = l->data;
            g_autoptr(JsonNode) root_node = mcp_root_to_json (root);
            json_builder_add_value (builder, g_steal_pointer (&root_node));
        }

        json_builder_end_array (builder);
        json_builder_end_object (builder);
        result = json_builder_get_root (builder);
        g_object_unref (builder);

        send_response (self, id, result);
    }
    else if (g_strcmp0 (method, "ping") == 0)
    {
        /* Respond to ping from server */
        JsonBuilder *builder;
        JsonNode *result;

        builder = json_builder_new ();
        json_builder_begin_object (builder);
        json_builder_end_object (builder);
        result = json_builder_get_root (builder);
        g_object_unref (builder);

        send_response (self, id, result);
    }
    else
    {
        /* Unknown method */
        send_error_response (self, id, MCP_ERROR_METHOD_NOT_FOUND,
                             "Method not found", NULL);
    }
}

/* Tasks API Implementation */

void
mcp_client_get_task_async (McpClient           *self,
                           const gchar         *task_id,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (task_id != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_get_task_async);
    g_task_set_task_data (task, GINT_TO_POINTER (11), NULL); /* Mark as tasks/get */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("tasks/get", id);

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "taskId");
    json_builder_add_string_value (builder, task_id);
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    mcp_request_set_params (request, g_steal_pointer (&params));
    send_request (self, request, task);
    g_object_unref (task);
}

McpTask *
mcp_client_get_task_finish (McpClient     *self,
                            GAsyncResult  *result,
                            GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

void
mcp_client_get_task_result_async (McpClient           *self,
                                  const gchar         *task_id,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (task_id != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_get_task_result_async);
    g_task_set_task_data (task, GINT_TO_POINTER (12), NULL); /* Mark as tasks/result */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("tasks/result", id);

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "taskId");
    json_builder_add_string_value (builder, task_id);
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    mcp_request_set_params (request, g_steal_pointer (&params));
    send_request (self, request, task);
    g_object_unref (task);
}

McpToolResult *
mcp_client_get_task_result_finish (McpClient     *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

void
mcp_client_cancel_task_async (McpClient           *self,
                              const gchar         *task_id,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (task_id != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_cancel_task_async);
    g_task_set_task_data (task, GINT_TO_POINTER (13), NULL); /* Mark as tasks/cancel */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("tasks/cancel", id);

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "taskId");
    json_builder_add_string_value (builder, task_id);
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    mcp_request_set_params (request, g_steal_pointer (&params));
    send_request (self, request, task);
    g_object_unref (task);
}

gboolean
mcp_client_cancel_task_finish (McpClient     *self,
                               GAsyncResult  *result,
                               GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

void
mcp_client_list_tasks_async (McpClient           *self,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_list_tasks_async);
    g_task_set_task_data (task, GINT_TO_POINTER (14), NULL); /* Mark as tasks/list */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("tasks/list", id);

    send_request (self, request, task);
    g_object_unref (task);
}

GList *
mcp_client_list_tasks_finish (McpClient     *self,
                              GAsyncResult  *result,
                              GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

/* ========================================================================== */
/* Sampling Response API                                                      */
/* ========================================================================== */

/**
 * mcp_client_respond_sampling:
 * @self: an #McpClient
 * @request_id: the request ID from the sampling-requested signal
 * @result: (transfer full): the #McpSamplingResult
 *
 * Responds to a sampling request with a result.
 * Call this from the sampling-requested signal handler.
 */
void
mcp_client_respond_sampling (McpClient         *self,
                             const gchar       *request_id,
                             McpSamplingResult *result)
{
    g_autoptr(JsonNode) result_node = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (request_id != NULL);
    g_return_if_fail (result != NULL);

    result_node = mcp_sampling_result_to_json (result);
    send_response (self, request_id, g_steal_pointer (&result_node));

    mcp_sampling_result_unref (result);
}

/**
 * mcp_client_reject_sampling:
 * @self: an #McpClient
 * @request_id: the request ID from the sampling-requested signal
 * @error_code: the error code
 * @error_message: the error message
 *
 * Rejects a sampling request with an error.
 * Call this from the sampling-requested signal handler.
 */
void
mcp_client_reject_sampling (McpClient   *self,
                            const gchar *request_id,
                            gint         error_code,
                            const gchar *error_message)
{
    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (request_id != NULL);
    g_return_if_fail (error_message != NULL);

    send_error_response (self, request_id, error_code, error_message, NULL);
}

/* ========================================================================== */
/* Roots Management API                                                       */
/* ========================================================================== */

/**
 * mcp_client_add_root:
 * @self: an #McpClient
 * @root: (transfer none): the #McpRoot to add
 *
 * Adds a root to the client's root list.
 * The root is copied, so the caller retains ownership.
 */
void
mcp_client_add_root (McpClient *self,
                     McpRoot   *root)
{
    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (root != NULL);

    self->roots = g_list_append (self->roots, mcp_root_ref (root));
}

/**
 * mcp_client_remove_root:
 * @self: an #McpClient
 * @uri: the root URI to remove
 *
 * Removes a root from the client's root list.
 *
 * Returns: %TRUE if the root was found and removed
 */
gboolean
mcp_client_remove_root (McpClient   *self,
                        const gchar *uri)
{
    GList *l;

    g_return_val_if_fail (MCP_IS_CLIENT (self), FALSE);
    g_return_val_if_fail (uri != NULL, FALSE);

    for (l = self->roots; l != NULL; l = l->next)
    {
        McpRoot *root = l->data;

        if (g_strcmp0 (mcp_root_get_uri (root), uri) == 0)
        {
            self->roots = g_list_delete_link (self->roots, l);
            mcp_root_unref (root);
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * mcp_client_list_roots:
 * @self: an #McpClient
 *
 * Gets the list of roots.
 *
 * Returns: (transfer none) (element-type McpRoot): the roots list
 */
GList *
mcp_client_list_roots (McpClient *self)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    return self->roots;
}

/**
 * mcp_client_notify_roots_changed:
 * @self: an #McpClient
 *
 * Notifies the server that the roots list has changed.
 * Call this after adding or removing roots.
 */
void
mcp_client_notify_roots_changed (McpClient *self)
{
    g_autoptr(McpNotification) notif = NULL;
    g_autoptr(JsonNode) node = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));

    if (self->transport == NULL ||
        mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        return;
    }

    notif = mcp_notification_new ("notifications/roots/list_changed");
    node = mcp_message_to_json (MCP_MESSAGE (notif));

    mcp_transport_send_message_async (self->transport, node, NULL,
                                      send_message_cb, NULL);
}

/* ========================================================================== */
/* Completion Request API                                                     */
/* ========================================================================== */

/**
 * mcp_client_complete_async:
 * @self: an #McpClient
 * @ref_type: the reference type ("ref/prompt" or "ref/resource")
 * @ref_name: the prompt name or resource URI
 * @argument_name: the argument name being completed
 * @argument_value: the current argument value
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Requests completions from the server.
 */
void
mcp_client_complete_async (McpClient           *self,
                           const gchar         *ref_type,
                           const gchar         *ref_name,
                           const gchar         *argument_name,
                           const gchar         *argument_value,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_CLIENT (self));
    g_return_if_fail (ref_type != NULL);
    g_return_if_fail (ref_name != NULL);
    g_return_if_fail (argument_name != NULL);
    g_return_if_fail (argument_value != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_client_complete_async);
    g_task_set_task_data (task, GINT_TO_POINTER (15), NULL); /* Mark as completion/complete */

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Client not connected");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("completion/complete", id);

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    /* ref object */
    json_builder_set_member_name (builder, "ref");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, ref_type);
    if (g_strcmp0 (ref_type, "ref/prompt") == 0)
    {
        json_builder_set_member_name (builder, "name");
        json_builder_add_string_value (builder, ref_name);
    }
    else
    {
        json_builder_set_member_name (builder, "uri");
        json_builder_add_string_value (builder, ref_name);
    }
    json_builder_end_object (builder);

    /* argument object */
    json_builder_set_member_name (builder, "argument");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "name");
    json_builder_add_string_value (builder, argument_name);
    json_builder_set_member_name (builder, "value");
    json_builder_add_string_value (builder, argument_value);
    json_builder_end_object (builder);

    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    mcp_request_set_params (request, g_steal_pointer (&params));

    send_request (self, request, task);
    g_object_unref (task);
}

/**
 * mcp_client_complete_finish:
 * @self: an #McpClient
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous completion request.
 *
 * Returns: (transfer full) (nullable): the #McpCompletionResult, or %NULL on error
 */
McpCompletionResult *
mcp_client_complete_finish (McpClient     *self,
                            GAsyncResult  *result,
                            GError       **error)
{
    g_return_val_if_fail (MCP_IS_CLIENT (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}
