/*
 * mcp-server.c - MCP Server implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp-server.h"
#include "mcp-message.h"
#include "mcp-error.h"
#include "mcp-version.h"
#include "mcp-task.h"
#undef MCP_COMPILATION

/**
 * SECTION:mcp-server
 * @title: McpServer
 * @short_description: MCP server implementation
 *
 * #McpServer provides a complete MCP server implementation that can
 * handle tool calls, resource reads, and prompt gets from connected clients.
 */

/* Handler registration structure */
typedef struct
{
    gpointer        handler;
    gpointer        user_data;
    GDestroyNotify  destroy;
} HandlerData;

static void
handler_data_free (gpointer data)
{
    HandlerData *hd = data;
    if (hd->destroy != NULL && hd->user_data != NULL)
    {
        hd->destroy (hd->user_data);
    }
    g_free (hd);
}

struct _McpServer
{
    McpSession parent_instance;

    /* Server identity */
    gchar *instructions;

    /* Capabilities */
    McpServerCapabilities *capabilities;
    McpClientCapabilities *client_capabilities;

    /* Transport */
    McpTransport *transport;
    gulong        message_handler_id;
    gulong        state_handler_id;
    gulong        error_handler_id;

    /* Registered tools: name -> McpTool */
    GHashTable *tools;
    /* Tool handlers: name -> HandlerData */
    GHashTable *tool_handlers;

    /* Registered resources: uri -> McpResource */
    GHashTable *resources;
    /* Resource handlers: uri -> HandlerData */
    GHashTable *resource_handlers;
    /* Resource templates: uri_template -> McpResourceTemplate */
    GHashTable *resource_templates;
    /* Template handlers: uri_template -> HandlerData */
    GHashTable *template_handlers;
    /* Subscriptions: uri -> TRUE */
    GHashTable *subscriptions;

    /* Registered prompts: name -> McpPrompt */
    GHashTable *prompts;
    /* Prompt handlers: name -> HandlerData */
    GHashTable *prompt_handlers;

    /* Async tool handlers (Tasks API): name -> HandlerData */
    GHashTable *async_tool_handlers;

    /* Active tasks: task_id -> McpTask */
    GHashTable *tasks;
    /* Task results: task_id -> McpToolResult */
    GHashTable *task_results;
    /* Task counter for generating unique IDs */
    guint64 task_counter;

    /* Completion handler */
    gpointer        completion_handler;
    gpointer        completion_user_data;
    GDestroyNotify  completion_destroy;

    /* Start task for async initialization */
    GTask *start_task;

    /* Main loop for synchronous run */
    GMainLoop *main_loop;
    GError    *run_error;
};

G_DEFINE_TYPE (McpServer, mcp_server, MCP_TYPE_SESSION)

enum
{
    PROP_0,
    PROP_CAPABILITIES,
    PROP_CLIENT_CAPABILITIES,
    PROP_TRANSPORT,
    PROP_INSTRUCTIONS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

enum
{
    SIGNAL_TOOL_CALLED,
    SIGNAL_RESOURCE_READ,
    SIGNAL_PROMPT_REQUESTED,
    SIGNAL_CLIENT_INITIALIZED,
    SIGNAL_CLIENT_DISCONNECTED,
    SIGNAL_ROOTS_CHANGED,
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

static void handle_request      (McpServer  *self,
                                 McpRequest *request);
static void handle_notification (McpServer       *self,
                                 McpNotification *notification);
static void handle_response     (McpServer   *self,
                                 McpResponse *response);
static void handle_error        (McpServer        *self,
                                 McpErrorResponse *error);

/*
 * uri_matches_template:
 * @uri: the URI to match
 * @template: the URI template (RFC 6570 Level 1 style)
 *
 * Matches a URI against a simple URI template.
 * Supports simple {variable} substitutions where {variable}
 * matches any non-empty sequence of characters.
 *
 * Examples:
 *   uri_matches_template("file:///home/user/test.txt", "file:///{path}")
 *     -> TRUE
 *   uri_matches_template("http://example.com/api/users/123", "http://example.com/api/users/{id}")
 *     -> TRUE
 *
 * Returns: %TRUE if the URI matches the template
 */
static gboolean
uri_matches_template (const gchar *uri,
                      const gchar *template)
{
    const gchar *u;
    const gchar *t;

    if (uri == NULL || template == NULL)
    {
        return FALSE;
    }

    u = uri;
    t = template;

    while (*t != '\0')
    {
        if (*t == '{')
        {
            const gchar *brace_end;
            const gchar *next_literal;

            /* Find end of variable name */
            brace_end = strchr (t, '}');
            if (brace_end == NULL)
            {
                /* Invalid template */
                return FALSE;
            }

            /* Find the next literal part after the variable */
            next_literal = brace_end + 1;

            if (*next_literal == '\0')
            {
                /* Variable at end of template - match rest of URI if non-empty */
                return (*u != '\0');
            }

            /* Find where the next literal starts in the URI */
            {
                const gchar *literal_start;

                /* Search for this literal in the remaining URI */
                literal_start = strstr (u, next_literal);
                if (literal_start == NULL || literal_start == u)
                {
                    /* Variable must match at least one character */
                    return FALSE;
                }

                /* Advance past the matched variable content and literal */
                u = literal_start;
                t = next_literal;
            }
        }
        else
        {
            /* Literal character - must match exactly */
            if (*u != *t)
            {
                return FALSE;
            }
            u++;
            t++;
        }
    }

    /* Both should be at end */
    return (*u == '\0');
}

/*
 * Helper to get request params as a JsonObject.
 * Returns NULL if params are not present or not an object.
 */
static JsonObject *
get_request_params_object (McpRequest *request)
{
    JsonNode *params_node;

    params_node = mcp_request_get_params (request);
    if (params_node == NULL)
    {
        return NULL;
    }

    if (JSON_NODE_HOLDS_OBJECT (params_node))
    {
        return json_node_get_object (params_node);
    }

    return NULL;
}

static void send_response       (McpServer   *self,
                                 const gchar *id,
                                 JsonNode    *result);
static void send_error_response (McpServer   *self,
                                 const gchar *id,
                                 gint         code,
                                 const gchar *message,
                                 JsonNode    *data);
static void send_notification   (McpServer   *self,
                                 const gchar *method,
                                 JsonNode    *params);

static void
mcp_server_dispose (GObject *object)
{
    McpServer *self = MCP_SERVER (object);

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
    g_clear_object (&self->client_capabilities);
    g_clear_object (&self->start_task);

    g_clear_pointer (&self->tools, g_hash_table_unref);
    g_clear_pointer (&self->tool_handlers, g_hash_table_unref);
    g_clear_pointer (&self->resources, g_hash_table_unref);
    g_clear_pointer (&self->resource_handlers, g_hash_table_unref);
    g_clear_pointer (&self->resource_templates, g_hash_table_unref);
    g_clear_pointer (&self->template_handlers, g_hash_table_unref);
    g_clear_pointer (&self->subscriptions, g_hash_table_unref);
    g_clear_pointer (&self->prompts, g_hash_table_unref);
    g_clear_pointer (&self->prompt_handlers, g_hash_table_unref);

    g_clear_pointer (&self->async_tool_handlers, g_hash_table_unref);
    g_clear_pointer (&self->tasks, g_hash_table_unref);
    g_clear_pointer (&self->task_results, g_hash_table_unref);

    if (self->main_loop != NULL)
    {
        g_main_loop_quit (self->main_loop);
        g_clear_pointer (&self->main_loop, g_main_loop_unref);
    }

    g_clear_error (&self->run_error);

    G_OBJECT_CLASS (mcp_server_parent_class)->dispose (object);
}

static void
mcp_server_finalize (GObject *object)
{
    McpServer *self = MCP_SERVER (object);

    g_free (self->instructions);

    G_OBJECT_CLASS (mcp_server_parent_class)->finalize (object);
}

static void
mcp_server_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    McpServer *self = MCP_SERVER (object);

    switch (prop_id)
    {
        case PROP_CAPABILITIES:
            g_value_set_object (value, self->capabilities);
            break;
        case PROP_CLIENT_CAPABILITIES:
            g_value_set_object (value, self->client_capabilities);
            break;
        case PROP_TRANSPORT:
            g_value_set_object (value, self->transport);
            break;
        case PROP_INSTRUCTIONS:
            g_value_set_string (value, self->instructions);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_server_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    McpServer *self = MCP_SERVER (object);

    switch (prop_id)
    {
        case PROP_INSTRUCTIONS:
            g_free (self->instructions);
            self->instructions = g_value_dup_string (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_server_class_init (McpServerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = mcp_server_dispose;
    object_class->finalize = mcp_server_finalize;
    object_class->get_property = mcp_server_get_property;
    object_class->set_property = mcp_server_set_property;

    properties[PROP_CAPABILITIES] =
        g_param_spec_object ("capabilities",
                             "Capabilities",
                             "The server capabilities",
                             MCP_TYPE_SERVER_CAPABILITIES,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_CLIENT_CAPABILITIES] =
        g_param_spec_object ("client-capabilities",
                             "Client Capabilities",
                             "The connected client's capabilities",
                             MCP_TYPE_CLIENT_CAPABILITIES,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_TRANSPORT] =
        g_param_spec_object ("transport",
                             "Transport",
                             "The transport for communication",
                             MCP_TYPE_TRANSPORT,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_INSTRUCTIONS] =
        g_param_spec_string ("instructions",
                             "Instructions",
                             "Server instructions for the LLM",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    /**
     * McpServer::tool-called:
     * @self: the #McpServer
     * @name: the tool name
     * @arguments: the arguments
     *
     * Emitted when a tool is called.
     */
    signals[SIGNAL_TOOL_CALLED] =
        g_signal_new ("tool-called",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 2,
                      G_TYPE_STRING, JSON_TYPE_OBJECT);

    /**
     * McpServer::resource-read:
     * @self: the #McpServer
     * @uri: the resource URI
     *
     * Emitted when a resource is read.
     */
    signals[SIGNAL_RESOURCE_READ] =
        g_signal_new ("resource-read",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 1,
                      G_TYPE_STRING);

    /**
     * McpServer::prompt-requested:
     * @self: the #McpServer
     * @name: the prompt name
     * @arguments: the arguments
     *
     * Emitted when a prompt is requested.
     */
    signals[SIGNAL_PROMPT_REQUESTED] =
        g_signal_new ("prompt-requested",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 2,
                      G_TYPE_STRING, G_TYPE_HASH_TABLE);

    /**
     * McpServer::client-initialized:
     * @self: the #McpServer
     *
     * Emitted when a client has completed initialization.
     */
    signals[SIGNAL_CLIENT_INITIALIZED] =
        g_signal_new ("client-initialized",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 0);

    /**
     * McpServer::client-disconnected:
     * @self: the #McpServer
     *
     * Emitted when the client disconnects.
     */
    signals[SIGNAL_CLIENT_DISCONNECTED] =
        g_signal_new ("client-disconnected",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 0);

    /**
     * McpServer::roots-changed:
     * @self: the #McpServer
     *
     * Emitted when the client notifies that its roots list has changed.
     */
    signals[SIGNAL_ROOTS_CHANGED] =
        g_signal_new ("roots-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_NONE, 0);
}

static void
mcp_server_init (McpServer *self)
{
    self->capabilities = mcp_server_capabilities_new ();

    self->tools = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);
    self->tool_handlers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, handler_data_free);

    self->resources = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, g_object_unref);
    self->resource_handlers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                      g_free, handler_data_free);
    self->resource_templates = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                       g_free, g_object_unref);
    self->template_handlers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                      g_free, handler_data_free);
    self->subscriptions = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, NULL);

    self->prompts = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free, g_object_unref);
    self->prompt_handlers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                    g_free, handler_data_free);

    /* Tasks API */
    self->async_tool_handlers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                        g_free, handler_data_free);
    self->tasks = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free, g_object_unref);
    self->task_results = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free,
                                                 (GDestroyNotify) mcp_tool_result_unref);
    self->task_counter = 0;
}

/**
 * mcp_server_new:
 * @name: the server name
 * @version: the server version
 *
 * Creates a new MCP server.
 *
 * Returns: (transfer full): a new #McpServer
 */
McpServer *
mcp_server_new (const gchar *name,
                const gchar *version)
{
    McpServer *self;
    McpImplementation *impl;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (version != NULL, NULL);

    self = g_object_new (MCP_TYPE_SERVER, NULL);

    impl = mcp_implementation_new (name, version);
    mcp_session_set_local_implementation (MCP_SESSION (self), impl);
    g_object_unref (impl);

    return self;
}

/* Property getters */

McpServerCapabilities *
mcp_server_get_capabilities (McpServer *self)
{
    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);
    return self->capabilities;
}

McpClientCapabilities *
mcp_server_get_client_capabilities (McpServer *self)
{
    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);
    return self->client_capabilities;
}

void
mcp_server_set_instructions (McpServer   *self,
                             const gchar *instructions)
{
    g_return_if_fail (MCP_IS_SERVER (self));

    g_free (self->instructions);
    self->instructions = g_strdup (instructions);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INSTRUCTIONS]);
}

const gchar *
mcp_server_get_instructions (McpServer *self)
{
    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);
    return self->instructions;
}

/* Transport management */

void
mcp_server_set_transport (McpServer    *self,
                          McpTransport *transport)
{
    g_return_if_fail (MCP_IS_SERVER (self));
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
mcp_server_get_transport (McpServer *self)
{
    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);
    return self->transport;
}

/* Start/stop */

static void
connect_cb (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
    McpServer *self = MCP_SERVER (user_data);
    GError *error = NULL;

    if (!mcp_transport_connect_finish (MCP_TRANSPORT (source), result, &error))
    {
        if (self->start_task != NULL)
        {
            g_task_return_error (self->start_task, error);
            g_clear_object (&self->start_task);
        }
        else
        {
            g_error_free (error);
        }
        return;
    }

    /* Now in CONNECTING state, waiting for initialize request */
    mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_CONNECTING);
}

void
mcp_server_start_async (McpServer           *self,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
    g_return_if_fail (MCP_IS_SERVER (self));

    if (self->start_task != NULL)
    {
        g_task_report_new_error (self, callback, user_data, mcp_server_start_async,
                                 MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Server already starting");
        return;
    }

    if (self->transport == NULL)
    {
        g_task_report_new_error (self, callback, user_data, mcp_server_start_async,
                                 MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "No transport set");
        return;
    }

    self->start_task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (self->start_task, mcp_server_start_async);

    /* Connect transport */
    mcp_transport_connect_async (self->transport, cancellable, connect_cb, self);
}

gboolean
mcp_server_start_finish (McpServer     *self,
                         GAsyncResult  *result,
                         GError       **error)
{
    g_return_val_if_fail (MCP_IS_SERVER (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

gboolean
mcp_server_run (McpServer  *self,
                GError    **error)
{
    g_return_val_if_fail (MCP_IS_SERVER (self), FALSE);

    if (self->transport == NULL)
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                     "No transport set");
        return FALSE;
    }

    /* Connect synchronously */
    {
        mcp_transport_connect_async (self->transport, NULL, connect_cb, self);

        /* Run main loop until transport disconnects or errors */
        self->main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (self->main_loop);

        if (self->run_error != NULL)
        {
            g_propagate_error (error, self->run_error);
            self->run_error = NULL;
            return FALSE;
        }
    }

    return TRUE;
}

void
mcp_server_stop (McpServer *self)
{
    g_return_if_fail (MCP_IS_SERVER (self));

    mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_CLOSING);

    if (self->main_loop != NULL)
    {
        g_main_loop_quit (self->main_loop);
    }

    if (self->transport != NULL)
    {
        mcp_transport_disconnect_async (self->transport, NULL, NULL, NULL);
    }

    mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_DISCONNECTED);
}

/* Tool management */

void
mcp_server_add_tool (McpServer      *self,
                     McpTool        *tool,
                     McpToolHandler  handler,
                     gpointer        user_data,
                     GDestroyNotify  destroy)
{
    const gchar *name;
    HandlerData *hd;

    g_return_if_fail (MCP_IS_SERVER (self));
    g_return_if_fail (MCP_IS_TOOL (tool));

    name = mcp_tool_get_name (tool);

    g_hash_table_insert (self->tools, g_strdup (name), g_object_ref (tool));

    if (handler != NULL)
    {
        hd = g_new0 (HandlerData, 1);
        hd->handler = handler;
        hd->user_data = user_data;
        hd->destroy = destroy;
        g_hash_table_insert (self->tool_handlers, g_strdup (name), hd);
    }

    /* Enable tools capability if not already */
    if (!mcp_server_capabilities_get_tools (self->capabilities))
    {
        mcp_server_capabilities_set_tools (self->capabilities, TRUE, TRUE);
    }
}

gboolean
mcp_server_remove_tool (McpServer   *self,
                        const gchar *name)
{
    g_return_val_if_fail (MCP_IS_SERVER (self), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    g_hash_table_remove (self->tool_handlers, name);
    return g_hash_table_remove (self->tools, name);
}

GList *
mcp_server_list_tools (McpServer *self)
{
    GList *list = NULL;
    GHashTableIter iter;
    gpointer value;

    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);

    g_hash_table_iter_init (&iter, self->tools);
    while (g_hash_table_iter_next (&iter, NULL, &value))
    {
        list = g_list_prepend (list, g_object_ref (value));
    }

    return g_list_reverse (list);
}

/* Resource management */

void
mcp_server_add_resource (McpServer          *self,
                         McpResource        *resource,
                         McpResourceHandler  handler,
                         gpointer            user_data,
                         GDestroyNotify      destroy)
{
    const gchar *uri;
    HandlerData *hd;

    g_return_if_fail (MCP_IS_SERVER (self));
    g_return_if_fail (MCP_IS_RESOURCE (resource));

    uri = mcp_resource_get_uri (resource);

    g_hash_table_insert (self->resources, g_strdup (uri), g_object_ref (resource));

    if (handler != NULL)
    {
        hd = g_new0 (HandlerData, 1);
        hd->handler = handler;
        hd->user_data = user_data;
        hd->destroy = destroy;
        g_hash_table_insert (self->resource_handlers, g_strdup (uri), hd);
    }

    /* Enable resources capability if not already */
    if (!mcp_server_capabilities_get_resources (self->capabilities))
    {
        mcp_server_capabilities_set_resources (self->capabilities, TRUE, TRUE, TRUE);
    }
}

void
mcp_server_add_resource_template (McpServer            *self,
                                  McpResourceTemplate  *templ,
                                  McpResourceHandler    handler,
                                  gpointer              user_data,
                                  GDestroyNotify        destroy)
{
    const gchar *uri_template;
    HandlerData *hd;

    g_return_if_fail (MCP_IS_SERVER (self));
    g_return_if_fail (MCP_IS_RESOURCE_TEMPLATE (templ));

    uri_template = mcp_resource_template_get_uri_template (templ);

    g_hash_table_insert (self->resource_templates, g_strdup (uri_template),
                         g_object_ref (templ));

    if (handler != NULL)
    {
        hd = g_new0 (HandlerData, 1);
        hd->handler = handler;
        hd->user_data = user_data;
        hd->destroy = destroy;
        g_hash_table_insert (self->template_handlers, g_strdup (uri_template), hd);
    }

    /* Enable resources capability if not already */
    if (!mcp_server_capabilities_get_resources (self->capabilities))
    {
        mcp_server_capabilities_set_resources (self->capabilities, TRUE, TRUE, TRUE);
    }
}

gboolean
mcp_server_remove_resource (McpServer   *self,
                            const gchar *uri)
{
    g_return_val_if_fail (MCP_IS_SERVER (self), FALSE);
    g_return_val_if_fail (uri != NULL, FALSE);

    g_hash_table_remove (self->resource_handlers, uri);
    g_hash_table_remove (self->subscriptions, uri);
    return g_hash_table_remove (self->resources, uri);
}

GList *
mcp_server_list_resources (McpServer *self)
{
    GList *list = NULL;
    GHashTableIter iter;
    gpointer value;

    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);

    g_hash_table_iter_init (&iter, self->resources);
    while (g_hash_table_iter_next (&iter, NULL, &value))
    {
        list = g_list_prepend (list, g_object_ref (value));
    }

    return g_list_reverse (list);
}

GList *
mcp_server_list_resource_templates (McpServer *self)
{
    GList *list = NULL;
    GHashTableIter iter;
    gpointer value;

    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);

    g_hash_table_iter_init (&iter, self->resource_templates);
    while (g_hash_table_iter_next (&iter, NULL, &value))
    {
        list = g_list_prepend (list, g_object_ref (value));
    }

    return g_list_reverse (list);
}

void
mcp_server_notify_resource_updated (McpServer   *self,
                                    const gchar *uri)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;

    g_return_if_fail (MCP_IS_SERVER (self));
    g_return_if_fail (uri != NULL);

    /* Only send if subscribed */
    if (!g_hash_table_contains (self->subscriptions, uri))
    {
        return;
    }

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "uri");
    json_builder_add_string_value (builder, uri);
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    send_notification (self, "notifications/resources/updated", params);
}

/* Prompt management */

void
mcp_server_add_prompt (McpServer        *self,
                       McpPrompt        *prompt,
                       McpPromptHandler  handler,
                       gpointer          user_data,
                       GDestroyNotify    destroy)
{
    const gchar *name;
    HandlerData *hd;

    g_return_if_fail (MCP_IS_SERVER (self));
    g_return_if_fail (MCP_IS_PROMPT (prompt));

    name = mcp_prompt_get_name (prompt);

    g_hash_table_insert (self->prompts, g_strdup (name), g_object_ref (prompt));

    if (handler != NULL)
    {
        hd = g_new0 (HandlerData, 1);
        hd->handler = handler;
        hd->user_data = user_data;
        hd->destroy = destroy;
        g_hash_table_insert (self->prompt_handlers, g_strdup (name), hd);
    }

    /* Enable prompts capability if not already */
    if (!mcp_server_capabilities_get_prompts (self->capabilities))
    {
        mcp_server_capabilities_set_prompts (self->capabilities, TRUE, TRUE);
    }
}

gboolean
mcp_server_remove_prompt (McpServer   *self,
                          const gchar *name)
{
    g_return_val_if_fail (MCP_IS_SERVER (self), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    g_hash_table_remove (self->prompt_handlers, name);
    return g_hash_table_remove (self->prompts, name);
}

GList *
mcp_server_list_prompts (McpServer *self)
{
    GList *list = NULL;
    GHashTableIter iter;
    gpointer value;

    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);

    g_hash_table_iter_init (&iter, self->prompts);
    while (g_hash_table_iter_next (&iter, NULL, &value))
    {
        list = g_list_prepend (list, g_object_ref (value));
    }

    return g_list_reverse (list);
}

/* Notifications */

void
mcp_server_notify_tools_changed (McpServer *self)
{
    g_return_if_fail (MCP_IS_SERVER (self));

    send_notification (self, "notifications/tools/list_changed", NULL);
}

void
mcp_server_notify_resources_changed (McpServer *self)
{
    g_return_if_fail (MCP_IS_SERVER (self));

    send_notification (self, "notifications/resources/list_changed", NULL);
}

void
mcp_server_notify_prompts_changed (McpServer *self)
{
    g_return_if_fail (MCP_IS_SERVER (self));

    send_notification (self, "notifications/prompts/list_changed", NULL);
}

void
mcp_server_emit_log (McpServer   *self,
                     McpLogLevel  level,
                     const gchar *logger,
                     JsonNode    *data)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    const gchar *level_str;

    g_return_if_fail (MCP_IS_SERVER (self));

    if (!mcp_server_capabilities_get_logging (self->capabilities))
    {
        return;
    }

    level_str = mcp_log_level_to_string (level);

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "level");
    json_builder_add_string_value (builder, level_str);
    if (logger != NULL)
    {
        json_builder_set_member_name (builder, "logger");
        json_builder_add_string_value (builder, logger);
    }
    json_builder_set_member_name (builder, "data");
    json_builder_add_value (builder, json_node_copy (data));
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    send_notification (self, "notifications/message", params);
}

void
mcp_server_send_progress (McpServer   *self,
                          const gchar *token,
                          gdouble      progress,
                          gint64       total)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;

    g_return_if_fail (MCP_IS_SERVER (self));
    g_return_if_fail (token != NULL);

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "progressToken");
    json_builder_add_string_value (builder, token);
    json_builder_set_member_name (builder, "progress");
    json_builder_add_double_value (builder, progress);
    if (total >= 0)
    {
        json_builder_set_member_name (builder, "total");
        json_builder_add_int_value (builder, total);
    }
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    send_notification (self, "notifications/progress", params);
}

/* Transport callbacks */

static void
on_message_received (McpTransport *transport,
                     JsonNode     *message,
                     gpointer      user_data)
{
    McpServer *self = MCP_SERVER (user_data);
    g_autoptr(McpMessage) msg = NULL;
    g_autoptr(GError) error = NULL;

    msg = mcp_message_new_from_json (message, &error);
    if (msg == NULL)
    {
        g_warning ("Failed to parse message: %s", error->message);
        send_error_response (self, NULL, MCP_ERROR_PARSE_ERROR,
                             error->message, NULL);
        return;
    }

    switch (mcp_message_get_message_type (msg))
    {
        case MCP_MESSAGE_TYPE_REQUEST:
            handle_request (self, MCP_REQUEST (msg));
            break;
        case MCP_MESSAGE_TYPE_NOTIFICATION:
            handle_notification (self, MCP_NOTIFICATION (msg));
            break;
        case MCP_MESSAGE_TYPE_RESPONSE:
            handle_response (self, MCP_RESPONSE (msg));
            break;
        case MCP_MESSAGE_TYPE_ERROR:
            handle_error (self, MCP_ERROR_RESPONSE (msg));
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
    McpServer *self = MCP_SERVER (user_data);

    if (new_state == MCP_TRANSPORT_STATE_DISCONNECTED)
    {
        mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_DISCONNECTED);
        g_signal_emit (self, signals[SIGNAL_CLIENT_DISCONNECTED], 0);
        if (self->main_loop != NULL)
        {
            g_main_loop_quit (self->main_loop);
        }
    }
    else if (new_state == MCP_TRANSPORT_STATE_ERROR)
    {
        mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_ERROR);
        if (self->main_loop != NULL)
        {
            g_main_loop_quit (self->main_loop);
        }
    }
}

static void
on_transport_error (McpTransport *transport,
                    GError       *error,
                    gpointer      user_data)
{
    McpServer *self = MCP_SERVER (user_data);

    g_warning ("Transport error: %s", error->message);

    if (self->run_error == NULL)
    {
        self->run_error = g_error_copy (error);
    }

    if (self->start_task != NULL)
    {
        g_task_return_error (self->start_task, g_error_copy (error));
        g_clear_object (&self->start_task);
    }
}

/* Message sending helpers */

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

static void
send_response (McpServer   *self,
               const gchar *id,
               JsonNode    *result)
{
    g_autoptr(McpResponse) response = NULL;
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(JsonNode) result_owned = result;  /* Take ownership of result */

    if (self->transport == NULL)
    {
        return;
    }

    /* mcp_response_new takes ownership of the node */
    response = mcp_response_new (id, g_steal_pointer (&result_owned));
    node = mcp_message_to_json (MCP_MESSAGE (response));

    mcp_transport_send_message_async (self->transport, node, NULL,
                                      send_message_cb, NULL);
}

static void
send_error_response (McpServer   *self,
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
send_notification (McpServer   *self,
                   const gchar *method,
                   JsonNode    *params)
{
    g_autoptr(McpNotification) notif = NULL;
    g_autoptr(JsonNode) node = NULL;

    if (self->transport == NULL ||
        mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        return;
    }

    notif = mcp_notification_new (method);
    if (params != NULL)
    {
        /* Copy the node since mcp_notification_set_params takes ownership */
        mcp_notification_set_params (notif, json_node_copy (params));
    }
    node = mcp_message_to_json (MCP_MESSAGE (notif));

    mcp_transport_send_message_async (self->transport, node, NULL,
                                      send_message_cb, NULL);
}

/* Request handlers */

static void
handle_initialize (McpServer  *self,
                   McpRequest *request)
{
    JsonObject *params;
    JsonNode *cap_node;
    g_autoptr(GError) error = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) result = NULL;
    McpImplementation *impl;

    params = get_request_params_object (request);
    if (params == NULL)
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing params", NULL);
        return;
    }

    /* Parse client capabilities */
    if (json_object_has_member (params, "capabilities"))
    {
        cap_node = json_object_get_member (params, "capabilities");
        self->client_capabilities = mcp_client_capabilities_new_from_json (cap_node, &error);
        if (self->client_capabilities == NULL)
        {
            g_warning ("Failed to parse client capabilities: %s", error->message);
            self->client_capabilities = mcp_client_capabilities_new ();
        }
    }
    else
    {
        self->client_capabilities = mcp_client_capabilities_new ();
    }

    /* Parse client info */
    if (json_object_has_member (params, "clientInfo"))
    {
        JsonNode *info_node = json_object_get_member (params, "clientInfo");
        McpImplementation *client_impl;

        client_impl = mcp_implementation_new_from_json (info_node, NULL);
        if (client_impl != NULL)
        {
            mcp_session_set_remote_implementation (MCP_SESSION (self), client_impl);
            g_object_unref (client_impl);
        }
    }

    /* Set protocol version */
    mcp_session_set_protocol_version (MCP_SESSION (self), MCP_PROTOCOL_VERSION);

    /* Move to initializing state */
    mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_INITIALIZING);

    /* Build response */
    impl = mcp_session_get_local_implementation (MCP_SESSION (self));

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "protocolVersion");
    json_builder_add_string_value (builder, MCP_PROTOCOL_VERSION);

    json_builder_set_member_name (builder, "capabilities");
    {
        g_autoptr(JsonNode) caps = mcp_server_capabilities_to_json (self->capabilities);
        json_builder_add_value (builder, g_steal_pointer (&caps));
    }

    json_builder_set_member_name (builder, "serverInfo");
    {
        g_autoptr(JsonNode) info = mcp_implementation_to_json (impl);
        json_builder_add_value (builder, g_steal_pointer (&info));
    }

    if (self->instructions != NULL)
    {
        json_builder_set_member_name (builder, "instructions");
        json_builder_add_string_value (builder, self->instructions);
    }

    json_builder_end_object (builder);
    result = json_builder_get_root (builder);

    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
}

static void
handle_tools_list (McpServer  *self,
                   McpRequest *request)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) result = NULL;
    GHashTableIter iter;
    gpointer value;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "tools");
    json_builder_begin_array (builder);

    g_hash_table_iter_init (&iter, self->tools);
    while (g_hash_table_iter_next (&iter, NULL, &value))
    {
        McpTool *tool = MCP_TOOL (value);
        g_autoptr(JsonNode) tool_node = mcp_tool_to_json (tool);
        json_builder_add_value (builder, g_steal_pointer (&tool_node));
    }

    json_builder_end_array (builder);
    json_builder_end_object (builder);
    result = json_builder_get_root (builder);

    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
}

static void
handle_tools_call (McpServer  *self,
                   McpRequest *request)
{
    JsonObject *params;
    const gchar *name;
    JsonObject *arguments = NULL;
    HandlerData *hd;
    HandlerData *async_hd;
    g_autoptr(McpToolResult) tool_result = NULL;
    g_autoptr(JsonNode) result = NULL;

    params = get_request_params_object (request);
    if (params == NULL || !json_object_has_member (params, "name"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing tool name", NULL);
        return;
    }

    name = json_object_get_string_member (params, "name");

    if (!g_hash_table_contains (self->tools, name))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_METHOD_NOT_FOUND,
                             "Unknown tool", NULL);
        return;
    }

    if (json_object_has_member (params, "arguments"))
    {
        arguments = json_object_get_object_member (params, "arguments");
    }

    g_signal_emit (self, signals[SIGNAL_TOOL_CALLED], 0, name, arguments);

    /* First check for async tool handler (Tasks API) */
    async_hd = g_hash_table_lookup (self->async_tool_handlers, name);
    if (async_hd != NULL && async_hd->handler != NULL)
    {
        McpAsyncToolHandler async_handler;
        g_autofree gchar *task_id = NULL;
        g_autoptr(McpTask) task = NULL;
        g_autoptr(JsonBuilder) builder = NULL;

        /* Generate unique task ID */
        task_id = g_strdup_printf ("task-%lu", ++self->task_counter);

        /* Create new task */
        task = mcp_task_new (task_id, MCP_TASK_STATUS_WORKING);
        mcp_task_set_status_message (task, "Processing");

        /* Store task */
        g_hash_table_insert (self->tasks, g_strdup (task_id), g_object_ref (task));

        /* Call async handler */
        async_handler = (McpAsyncToolHandler) async_hd->handler;
        tool_result = async_handler (self, task, name, arguments, async_hd->user_data);

        if (tool_result != NULL)
        {
            /* Handler completed synchronously */
            mcp_task_set_status (task, MCP_TASK_STATUS_COMPLETED);
            mcp_task_set_status_message (task, "Completed synchronously");
            mcp_task_update_timestamp (task);

            /* Store result */
            g_hash_table_insert (self->task_results, g_strdup (task_id),
                                 mcp_tool_result_ref (tool_result));

            /* Return result with task info */
            builder = json_builder_new ();
            json_builder_begin_object (builder);

            /* Add content from tool result */
            {
                g_autoptr(JsonNode) tool_node = mcp_tool_result_to_json (tool_result);
                JsonObject *tool_obj = json_node_get_object (tool_node);

                if (json_object_has_member (tool_obj, "content"))
                {
                    JsonArray *content = json_object_get_array_member (tool_obj, "content");
                    JsonNode *arr_node = json_node_new (JSON_NODE_ARRAY);
                    json_builder_set_member_name (builder, "content");
                    /* json_node_set_array refs the array, so don't add extra ref */
                    json_node_set_array (arr_node, content);
                    json_builder_add_value (builder, arr_node);
                }

                if (json_object_has_member (tool_obj, "isError"))
                {
                    json_builder_set_member_name (builder, "isError");
                    json_builder_add_boolean_value (builder, json_object_get_boolean_member (tool_obj, "isError"));
                }
            }

            /* Add task info */
            json_builder_set_member_name (builder, "task");
            {
                g_autoptr(JsonNode) task_node = mcp_task_to_json (task);
                json_builder_add_value (builder, g_steal_pointer (&task_node));
            }

            json_builder_end_object (builder);
            result = json_builder_get_root (builder);
        }
        else
        {
            /* Handler is processing asynchronously */
            /* Return task reference */
            builder = json_builder_new ();
            json_builder_begin_object (builder);
            json_builder_set_member_name (builder, "task");
            {
                g_autoptr(JsonNode) task_node = mcp_task_to_json (task);
                json_builder_add_value (builder, g_steal_pointer (&task_node));
            }
            json_builder_end_object (builder);
            result = json_builder_get_root (builder);
        }

        send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
        return;
    }

    /* Regular synchronous tool handler */
    hd = g_hash_table_lookup (self->tool_handlers, name);
    if (hd != NULL && hd->handler != NULL)
    {
        McpToolHandler handler;
        handler = (McpToolHandler) hd->handler;
        tool_result = handler (self, name, arguments, hd->user_data);
    }
    else
    {
        /* No handler - return empty result */
        tool_result = mcp_tool_result_new (FALSE);
        mcp_tool_result_add_text (tool_result, "");
    }

    result = mcp_tool_result_to_json (tool_result);
    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
}

static void
handle_resources_list (McpServer  *self,
                       McpRequest *request)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) result = NULL;
    GHashTableIter iter;
    gpointer value;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "resources");
    json_builder_begin_array (builder);

    g_hash_table_iter_init (&iter, self->resources);
    while (g_hash_table_iter_next (&iter, NULL, &value))
    {
        McpResource *resource = MCP_RESOURCE (value);
        g_autoptr(JsonNode) res_node = mcp_resource_to_json (resource);
        json_builder_add_value (builder, g_steal_pointer (&res_node));
    }

    json_builder_end_array (builder);
    json_builder_end_object (builder);
    result = json_builder_get_root (builder);

    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
}

static void
handle_resources_templates_list (McpServer  *self,
                                 McpRequest *request)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) result = NULL;
    GHashTableIter iter;
    gpointer value;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "resourceTemplates");
    json_builder_begin_array (builder);

    g_hash_table_iter_init (&iter, self->resource_templates);
    while (g_hash_table_iter_next (&iter, NULL, &value))
    {
        McpResourceTemplate *templ = MCP_RESOURCE_TEMPLATE (value);
        g_autoptr(JsonNode) templ_node = mcp_resource_template_to_json (templ);
        json_builder_add_value (builder, g_steal_pointer (&templ_node));
    }

    json_builder_end_array (builder);
    json_builder_end_object (builder);
    result = json_builder_get_root (builder);

    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
}

static void
handle_resources_read (McpServer  *self,
                       McpRequest *request)
{
    JsonObject *params;
    const gchar *uri;
    HandlerData *hd;
    McpResourceHandler handler;
    GList *contents = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) result = NULL;
    GList *l;

    params = get_request_params_object (request);
    if (params == NULL || !json_object_has_member (params, "uri"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing resource URI", NULL);
        return;
    }

    uri = json_object_get_string_member (params, "uri");

    g_signal_emit (self, signals[SIGNAL_RESOURCE_READ], 0, uri);

    /* First try direct resource handlers */
    hd = g_hash_table_lookup (self->resource_handlers, uri);
    if (hd != NULL && hd->handler != NULL)
    {
        handler = (McpResourceHandler) hd->handler;
        contents = handler (self, uri, hd->user_data);
    }

    /* If no direct handler, try template matching */
    if (contents == NULL)
    {
        GHashTableIter iter;
        const gchar *template_uri;
        McpResourceTemplate *templ;

        g_hash_table_iter_init (&iter, self->resource_templates);
        while (g_hash_table_iter_next (&iter, (gpointer *)&template_uri, (gpointer *)&templ))
        {
            if (uri_matches_template (uri, template_uri))
            {
                hd = g_hash_table_lookup (self->template_handlers, template_uri);
                if (hd != NULL && hd->handler != NULL)
                {
                    handler = (McpResourceHandler) hd->handler;
                    contents = handler (self, uri, hd->user_data);
                    break;
                }
            }
        }
    }

    if (contents == NULL)
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Resource not found", NULL);
        return;
    }

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "contents");
    json_builder_begin_array (builder);

    for (l = contents; l != NULL; l = l->next)
    {
        McpResourceContents *c = l->data;
        g_autoptr(JsonNode) c_node = mcp_resource_contents_to_json (c);
        json_builder_add_value (builder, g_steal_pointer (&c_node));
    }

    json_builder_end_array (builder);
    json_builder_end_object (builder);
    result = json_builder_get_root (builder);

    g_list_free_full (contents, (GDestroyNotify) mcp_resource_contents_unref);

    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
}

static void
handle_resources_subscribe (McpServer  *self,
                            McpRequest *request)
{
    JsonObject *params;
    const gchar *uri;

    params = get_request_params_object (request);
    if (params == NULL || !json_object_has_member (params, "uri"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing resource URI", NULL);
        return;
    }

    uri = json_object_get_string_member (params, "uri");

    g_hash_table_insert (self->subscriptions, g_strdup (uri), GINT_TO_POINTER (TRUE));

    send_response (self, mcp_request_get_id (request), json_node_new (JSON_NODE_OBJECT));
}

static void
handle_resources_unsubscribe (McpServer  *self,
                              McpRequest *request)
{
    JsonObject *params;
    const gchar *uri;

    params = get_request_params_object (request);
    if (params == NULL || !json_object_has_member (params, "uri"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing resource URI", NULL);
        return;
    }

    uri = json_object_get_string_member (params, "uri");

    g_hash_table_remove (self->subscriptions, uri);

    send_response (self, mcp_request_get_id (request), json_node_new (JSON_NODE_OBJECT));
}

static void
handle_prompts_list (McpServer  *self,
                     McpRequest *request)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) result = NULL;
    GHashTableIter iter;
    gpointer value;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "prompts");
    json_builder_begin_array (builder);

    g_hash_table_iter_init (&iter, self->prompts);
    while (g_hash_table_iter_next (&iter, NULL, &value))
    {
        McpPrompt *prompt = MCP_PROMPT (value);
        g_autoptr(JsonNode) prompt_node = mcp_prompt_to_json (prompt);
        json_builder_add_value (builder, g_steal_pointer (&prompt_node));
    }

    json_builder_end_array (builder);
    json_builder_end_object (builder);
    result = json_builder_get_root (builder);

    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
}

static void
handle_prompts_get (McpServer  *self,
                    McpRequest *request)
{
    JsonObject *params;
    const gchar *name;
    JsonObject *args_obj = NULL;
    GHashTable *arguments = NULL;
    HandlerData *hd;
    McpPromptHandler handler;
    g_autoptr(McpPromptResult) prompt_result = NULL;
    g_autoptr(JsonNode) result = NULL;

    params = get_request_params_object (request);
    if (params == NULL || !json_object_has_member (params, "name"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing prompt name", NULL);
        return;
    }

    name = json_object_get_string_member (params, "name");

    if (!g_hash_table_contains (self->prompts, name))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_METHOD_NOT_FOUND,
                             "Unknown prompt", NULL);
        return;
    }

    if (json_object_has_member (params, "arguments"))
    {
        args_obj = json_object_get_object_member (params, "arguments");
        if (args_obj != NULL)
        {
            GList *members, *l;

            arguments = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
            members = json_object_get_members (args_obj);
            for (l = members; l != NULL; l = l->next)
            {
                const gchar *key = l->data;
                const gchar *val = json_object_get_string_member (args_obj, key);
                g_hash_table_insert (arguments, g_strdup (key), g_strdup (val));
            }
            g_list_free (members);
        }
    }

    g_signal_emit (self, signals[SIGNAL_PROMPT_REQUESTED], 0, name, arguments);

    hd = g_hash_table_lookup (self->prompt_handlers, name);
    if (hd != NULL && hd->handler != NULL)
    {
        handler = (McpPromptHandler) hd->handler;
        prompt_result = handler (self, name, arguments, hd->user_data);
    }
    else
    {
        /* No handler - return empty result */
        prompt_result = mcp_prompt_result_new (NULL);
    }

    if (arguments != NULL)
    {
        g_hash_table_unref (arguments);
    }

    result = mcp_prompt_result_to_json (prompt_result);
    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
}

static void
handle_ping (McpServer  *self,
             McpRequest *request)
{
    send_response (self, mcp_request_get_id (request), json_node_new (JSON_NODE_OBJECT));
}

static void
handle_completion_complete (McpServer  *self,
                            McpRequest *request)
{
    JsonObject *params;
    JsonObject *ref;
    JsonObject *argument;
    const gchar *ref_type;
    const gchar *ref_name;
    const gchar *arg_name = NULL;
    const gchar *arg_value;
    McpCompletionHandler handler;
    g_autoptr(McpCompletionResult) completion_result = NULL;
    g_autoptr(JsonNode) result = NULL;

    params = get_request_params_object (request);
    if (params == NULL)
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing params", NULL);
        return;
    }

    /* Parse ref object */
    if (!json_object_has_member (params, "ref"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing ref", NULL);
        return;
    }

    ref = json_object_get_object_member (params, "ref");
    if (ref == NULL || !json_object_has_member (ref, "type"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Invalid ref", NULL);
        return;
    }

    ref_type = json_object_get_string_member (ref, "type");

    /* Get name based on ref type */
    if (g_strcmp0 (ref_type, "ref/prompt") == 0)
    {
        if (!json_object_has_member (ref, "name"))
        {
            send_error_response (self, mcp_request_get_id (request),
                                 MCP_ERROR_INVALID_PARAMS,
                                 "Missing prompt name", NULL);
            return;
        }
        ref_name = json_object_get_string_member (ref, "name");
    }
    else if (g_strcmp0 (ref_type, "ref/resource") == 0)
    {
        if (!json_object_has_member (ref, "uri"))
        {
            send_error_response (self, mcp_request_get_id (request),
                                 MCP_ERROR_INVALID_PARAMS,
                                 "Missing resource uri", NULL);
            return;
        }
        ref_name = json_object_get_string_member (ref, "uri");
    }
    else
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Unknown ref type", NULL);
        return;
    }

    /* Parse argument object */
    if (!json_object_has_member (params, "argument"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing argument", NULL);
        return;
    }

    argument = json_object_get_object_member (params, "argument");
    if (argument == NULL || !json_object_has_member (argument, "value"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Invalid argument", NULL);
        return;
    }

    if (json_object_has_member (argument, "name"))
    {
        arg_name = json_object_get_string_member (argument, "name");
    }
    arg_value = json_object_get_string_member (argument, "value");

    /* Call completion handler if set */
    handler = (McpCompletionHandler) self->completion_handler;
    if (handler != NULL)
    {
        completion_result = handler (self, ref_type, ref_name, arg_name, arg_value,
                                     self->completion_user_data);
    }

    if (completion_result == NULL)
    {
        /* No handler or no result - return empty completion */
        completion_result = mcp_completion_result_new ();
    }

    result = mcp_completion_result_to_json (completion_result);
    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
}

/* Forward declarations for Tasks API handlers */
static void handle_tasks_get    (McpServer *self, McpRequest *request);
static void handle_tasks_result (McpServer *self, McpRequest *request);
static void handle_tasks_cancel (McpServer *self, McpRequest *request);
static void handle_tasks_list   (McpServer *self, McpRequest *request);

static void
handle_request (McpServer  *self,
                McpRequest *request)
{
    const gchar *method;

    method = mcp_request_get_method (request);

    if (g_strcmp0 (method, "initialize") == 0)
    {
        handle_initialize (self, request);
    }
    else if (g_strcmp0 (method, "ping") == 0)
    {
        handle_ping (self, request);
    }
    else if (g_strcmp0 (method, "tools/list") == 0)
    {
        handle_tools_list (self, request);
    }
    else if (g_strcmp0 (method, "tools/call") == 0)
    {
        handle_tools_call (self, request);
    }
    else if (g_strcmp0 (method, "resources/list") == 0)
    {
        handle_resources_list (self, request);
    }
    else if (g_strcmp0 (method, "resources/templates/list") == 0)
    {
        handle_resources_templates_list (self, request);
    }
    else if (g_strcmp0 (method, "resources/read") == 0)
    {
        handle_resources_read (self, request);
    }
    else if (g_strcmp0 (method, "resources/subscribe") == 0)
    {
        handle_resources_subscribe (self, request);
    }
    else if (g_strcmp0 (method, "resources/unsubscribe") == 0)
    {
        handle_resources_unsubscribe (self, request);
    }
    else if (g_strcmp0 (method, "prompts/list") == 0)
    {
        handle_prompts_list (self, request);
    }
    else if (g_strcmp0 (method, "prompts/get") == 0)
    {
        handle_prompts_get (self, request);
    }
    /* Completion API */
    else if (g_strcmp0 (method, "completion/complete") == 0)
    {
        handle_completion_complete (self, request);
    }
    /* Tasks API (experimental) */
    else if (g_strcmp0 (method, "tasks/get") == 0)
    {
        handle_tasks_get (self, request);
    }
    else if (g_strcmp0 (method, "tasks/result") == 0)
    {
        handle_tasks_result (self, request);
    }
    else if (g_strcmp0 (method, "tasks/cancel") == 0)
    {
        handle_tasks_cancel (self, request);
    }
    else if (g_strcmp0 (method, "tasks/list") == 0)
    {
        handle_tasks_list (self, request);
    }
    else
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_METHOD_NOT_FOUND,
                             "Unknown method", NULL);
    }
}

static void
handle_notification (McpServer       *self,
                     McpNotification *notification)
{
    const gchar *method;

    method = mcp_notification_get_method (notification);

    if (g_strcmp0 (method, "notifications/initialized") == 0)
    {
        /* Client has acknowledged initialization */
        mcp_session_set_state (MCP_SESSION (self), MCP_SESSION_STATE_READY);
        g_signal_emit (self, signals[SIGNAL_CLIENT_INITIALIZED], 0);

        if (self->start_task != NULL)
        {
            g_task_return_boolean (self->start_task, TRUE);
            g_clear_object (&self->start_task);
        }
    }
    else if (g_strcmp0 (method, "notifications/cancelled") == 0)
    {
        JsonNode *params_node;
        JsonObject *params;
        const gchar *request_id;
        GTask *task;

        params_node = mcp_notification_get_params (notification);
        if (params_node != NULL && JSON_NODE_HOLDS_OBJECT (params_node))
        {
            params = json_node_get_object (params_node);
            if (json_object_has_member (params, "requestId"))
            {
                request_id = json_object_get_string_member (params, "requestId");
                if (request_id != NULL)
                {
                    /* Try to cancel pending server-initiated request */
                    task = mcp_session_take_pending_request (MCP_SESSION (self), request_id);
                    if (task != NULL)
                    {
                        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                                 "Request cancelled by client");
                        g_object_unref (task);
                    }
                }
            }
        }
    }
    else if (g_strcmp0 (method, "notifications/roots/list_changed") == 0)
    {
        /* Client's roots list has changed */
        g_signal_emit (self, signals[SIGNAL_ROOTS_CHANGED], 0);
    }
    /* Other notifications can be ignored by server */
}

/*
 * handle_response:
 * @self: the server
 * @response: the response from client
 *
 * Handles responses to server-initiated requests (sampling, roots).
 */
static void
handle_response (McpServer   *self,
                 McpResponse *response)
{
    const gchar *id;
    GTask *task;

    id = mcp_response_get_id (response);
    if (id == NULL)
    {
        g_warning ("Received response without id");
        return;
    }

    task = mcp_session_take_pending_request (MCP_SESSION (self), id);
    if (task == NULL)
    {
        g_warning ("Received response for unknown request: %s", id);
        return;
    }

    /* Return the result node - caller should interpret it */
    g_task_return_pointer (task, json_node_copy (mcp_response_get_result (response)),
                           (GDestroyNotify)json_node_unref);
    g_object_unref (task);
}

/*
 * handle_error:
 * @self: the server
 * @error_response: the error response from client
 *
 * Handles error responses to server-initiated requests.
 */
static void
handle_error (McpServer        *self,
              McpErrorResponse *error_response)
{
    const gchar *id;
    GTask *task;
    gint code;
    const gchar *message;

    id = mcp_error_response_get_id (error_response);
    if (id == NULL)
    {
        g_warning ("Received error response without id");
        return;
    }

    task = mcp_session_take_pending_request (MCP_SESSION (self), id);
    if (task == NULL)
    {
        g_warning ("Received error response for unknown request: %s", id);
        return;
    }

    code = mcp_error_response_get_code (error_response);
    message = mcp_error_response_get_error_message (error_response);

    g_task_return_new_error (task, MCP_ERROR, code, "%s", message);
    g_object_unref (task);
}

/*
 * send_request:
 * @self: the server
 * @request: the request to send
 * @task: the task to complete when response arrives
 *
 * Sends a request to the client and tracks the pending task.
 */
static void
send_request (McpServer  *self,
              McpRequest *request,
              GTask      *task)
{
    g_autoptr(JsonNode) node = NULL;
    const gchar *id;

    id = mcp_request_get_id (request);
    mcp_session_add_pending_request (MCP_SESSION (self), id, task);

    node = mcp_message_to_json (MCP_MESSAGE (request));
    mcp_transport_send_message_async (self->transport, node, NULL, NULL, NULL);
}

/* Tasks API Implementation */

/**
 * mcp_server_add_async_tool:
 * @self: an #McpServer
 * @tool: (transfer none): the #McpTool to add
 * @handler: (scope notified) (nullable): the async handler function
 * @user_data: (closure): user data for @handler
 * @destroy: (destroy user_data): destroy notify for @user_data
 *
 * Adds a tool with asynchronous execution support (Tasks API).
 */
void
mcp_server_add_async_tool (McpServer          *self,
                           McpTool            *tool,
                           McpAsyncToolHandler handler,
                           gpointer            user_data,
                           GDestroyNotify      destroy)
{
    const gchar *name;
    HandlerData *hd;

    g_return_if_fail (MCP_IS_SERVER (self));
    g_return_if_fail (MCP_IS_TOOL (tool));

    name = mcp_tool_get_name (tool);

    /* Add tool to the regular tools list */
    g_hash_table_insert (self->tools, g_strdup (name), g_object_ref (tool));

    /* Add async handler */
    if (handler != NULL)
    {
        hd = g_new0 (HandlerData, 1);
        hd->handler = handler;
        hd->user_data = user_data;
        hd->destroy = destroy;
        g_hash_table_insert (self->async_tool_handlers, g_strdup (name), hd);
    }

    /* Enable tools capability if not already */
    if (!mcp_server_capabilities_get_tools (self->capabilities))
    {
        mcp_server_capabilities_set_tools (self->capabilities, TRUE, TRUE);
    }
}

/**
 * mcp_server_get_task:
 * @self: an #McpServer
 * @task_id: the task identifier
 *
 * Gets a task by its identifier.
 *
 * Returns: (transfer none) (nullable): the #McpTask, or %NULL if not found
 */
McpTask *
mcp_server_get_task (McpServer   *self,
                     const gchar *task_id)
{
    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);
    g_return_val_if_fail (task_id != NULL, NULL);

    return g_hash_table_lookup (self->tasks, task_id);
}

/**
 * mcp_server_list_tasks:
 * @self: an #McpServer
 *
 * Lists all active tasks.
 *
 * Returns: (transfer full) (element-type McpTask): a list of tasks
 */
GList *
mcp_server_list_tasks (McpServer *self)
{
    GList *list = NULL;
    GHashTableIter iter;
    gpointer value;

    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);

    g_hash_table_iter_init (&iter, self->tasks);
    while (g_hash_table_iter_next (&iter, NULL, &value))
    {
        list = g_list_prepend (list, g_object_ref (value));
    }

    return g_list_reverse (list);
}

/**
 * mcp_server_notify_task_status:
 * @self: an #McpServer
 * @task: the #McpTask that was updated
 *
 * Sends a task status notification to the client.
 */
void
mcp_server_notify_task_status (McpServer *self,
                               McpTask   *task)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autoptr(JsonNode) task_node = NULL;

    g_return_if_fail (MCP_IS_SERVER (self));
    g_return_if_fail (MCP_IS_TASK (task));

    task_node = mcp_task_to_json (task);

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "task");
    json_builder_add_value (builder, g_steal_pointer (&task_node));
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    send_notification (self, "notifications/tasks/status", params);
}

/**
 * mcp_server_complete_task:
 * @self: an #McpServer
 * @task_id: the task identifier
 * @result: (transfer full) (nullable): the tool result
 * @error_message: (nullable): an error message if the task failed
 *
 * Marks a task as completed with the given result.
 *
 * Returns: %TRUE if the task was found and completed
 */
gboolean
mcp_server_complete_task (McpServer     *self,
                          const gchar   *task_id,
                          McpToolResult *result,
                          const gchar   *error_message)
{
    McpTask *task;

    g_return_val_if_fail (MCP_IS_SERVER (self), FALSE);
    g_return_val_if_fail (task_id != NULL, FALSE);

    task = g_hash_table_lookup (self->tasks, task_id);
    if (task == NULL)
    {
        if (result != NULL)
        {
            mcp_tool_result_unref (result);
        }
        return FALSE;
    }

    /* Store the result */
    if (result != NULL)
    {
        g_hash_table_insert (self->task_results, g_strdup (task_id), result);
    }

    /* Update task status */
    if (error_message != NULL)
    {
        mcp_task_set_status (task, MCP_TASK_STATUS_FAILED);
        mcp_task_set_status_message (task, error_message);
    }
    else
    {
        mcp_task_set_status (task, MCP_TASK_STATUS_COMPLETED);
        mcp_task_set_status_message (task, "Completed successfully");
    }

    mcp_task_update_timestamp (task);

    /* Notify client */
    mcp_server_notify_task_status (self, task);

    return TRUE;
}

/**
 * mcp_server_fail_task:
 * @self: an #McpServer
 * @task_id: the task identifier
 * @error_message: the error message
 *
 * Marks a task as failed with the given error message.
 *
 * Returns: %TRUE if the task was found and failed
 */
gboolean
mcp_server_fail_task (McpServer   *self,
                      const gchar *task_id,
                      const gchar *error_message)
{
    return mcp_server_complete_task (self, task_id, NULL, error_message);
}

/**
 * mcp_server_update_task_status:
 * @self: an #McpServer
 * @task_id: the task identifier
 * @status: the new status
 * @message: (nullable): a status message
 *
 * Updates the status of a task and sends a notification.
 *
 * Returns: %TRUE if the task was found and updated
 */
gboolean
mcp_server_update_task_status (McpServer     *self,
                               const gchar   *task_id,
                               McpTaskStatus  status,
                               const gchar   *message)
{
    McpTask *task;

    g_return_val_if_fail (MCP_IS_SERVER (self), FALSE);
    g_return_val_if_fail (task_id != NULL, FALSE);

    task = g_hash_table_lookup (self->tasks, task_id);
    if (task == NULL)
    {
        return FALSE;
    }

    mcp_task_set_status (task, status);
    if (message != NULL)
    {
        mcp_task_set_status_message (task, message);
    }
    mcp_task_update_timestamp (task);

    /* Notify client */
    mcp_server_notify_task_status (self, task);

    return TRUE;
}

/**
 * mcp_server_cancel_task:
 * @self: an #McpServer
 * @task_id: the task identifier
 *
 * Cancels a running task.
 *
 * Returns: %TRUE if the task was found and cancelled
 */
gboolean
mcp_server_cancel_task (McpServer   *self,
                        const gchar *task_id)
{
    McpTask *task;

    g_return_val_if_fail (MCP_IS_SERVER (self), FALSE);
    g_return_val_if_fail (task_id != NULL, FALSE);

    task = g_hash_table_lookup (self->tasks, task_id);
    if (task == NULL)
    {
        return FALSE;
    }

    mcp_task_set_status (task, MCP_TASK_STATUS_CANCELLED);
    mcp_task_set_status_message (task, "Task cancelled by request");
    mcp_task_update_timestamp (task);

    /* Notify client */
    mcp_server_notify_task_status (self, task);

    return TRUE;
}

/* Task request handlers */

static void
handle_tasks_get (McpServer  *self,
                  McpRequest *request)
{
    JsonObject *params;
    const gchar *task_id;
    McpTask *task;
    g_autoptr(JsonNode) result = NULL;

    params = get_request_params_object (request);
    if (params == NULL || !json_object_has_member (params, "taskId"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing taskId", NULL);
        return;
    }

    task_id = json_object_get_string_member (params, "taskId");
    task = g_hash_table_lookup (self->tasks, task_id);

    if (task == NULL)
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Task not found", NULL);
        return;
    }

    result = mcp_task_to_json (task);
    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
}

static void
handle_tasks_result (McpServer  *self,
                     McpRequest *request)
{
    JsonObject *params;
    const gchar *task_id;
    McpTask *task;
    McpToolResult *result;
    g_autoptr(JsonNode) result_node = NULL;

    params = get_request_params_object (request);
    if (params == NULL || !json_object_has_member (params, "taskId"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing taskId", NULL);
        return;
    }

    task_id = json_object_get_string_member (params, "taskId");
    task = g_hash_table_lookup (self->tasks, task_id);

    if (task == NULL)
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Task not found", NULL);
        return;
    }

    /* Check if task is completed */
    if (mcp_task_get_status (task) != MCP_TASK_STATUS_COMPLETED &&
        mcp_task_get_status (task) != MCP_TASK_STATUS_FAILED)
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Task not yet completed", NULL);
        return;
    }

    result = g_hash_table_lookup (self->task_results, task_id);
    if (result != NULL)
    {
        result_node = mcp_tool_result_to_json (result);
    }
    else
    {
        /* Return empty result */
        g_autoptr(McpToolResult) empty = mcp_tool_result_new (FALSE);
        result_node = mcp_tool_result_to_json (empty);
    }

    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result_node));
}

static void
handle_tasks_cancel (McpServer  *self,
                     McpRequest *request)
{
    JsonObject *params;
    const gchar *task_id;

    params = get_request_params_object (request);
    if (params == NULL || !json_object_has_member (params, "taskId"))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Missing taskId", NULL);
        return;
    }

    task_id = json_object_get_string_member (params, "taskId");

    if (!mcp_server_cancel_task (self, task_id))
    {
        send_error_response (self, mcp_request_get_id (request),
                             MCP_ERROR_INVALID_PARAMS,
                             "Task not found", NULL);
        return;
    }

    send_response (self, mcp_request_get_id (request), json_node_new (JSON_NODE_OBJECT));
}

static void
handle_tasks_list (McpServer  *self,
                   McpRequest *request)
{
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) result = NULL;
    GHashTableIter iter;
    gpointer value;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "tasks");
    json_builder_begin_array (builder);

    g_hash_table_iter_init (&iter, self->tasks);
    while (g_hash_table_iter_next (&iter, NULL, &value))
    {
        McpTask *task = MCP_TASK (value);
        g_autoptr(JsonNode) task_node = mcp_task_to_json (task);
        json_builder_add_value (builder, g_steal_pointer (&task_node));
    }

    json_builder_end_array (builder);
    json_builder_end_object (builder);
    result = json_builder_get_root (builder);

    send_response (self, mcp_request_get_id (request), g_steal_pointer (&result));
}

/* ========================================================================== */
/* Sampling Request API                                                       */
/* ========================================================================== */

/**
 * mcp_server_request_sampling_async:
 * @self: an #McpServer
 * @messages: (element-type McpSamplingMessage): the messages list
 * @model_preferences: (nullable): model selection preferences
 * @system_prompt: (nullable): the system prompt
 * @max_tokens: maximum tokens to generate (-1 for no limit)
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Requests LLM sampling from the client.
 */
void
mcp_server_request_sampling_async (McpServer           *self,
                                   GList               *messages,
                                   McpModelPreferences *model_preferences,
                                   const gchar         *system_prompt,
                                   gint64               max_tokens,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    g_autoptr(JsonNode) params = NULL;
    g_autofree gchar *id = NULL;
    GList *l;

    g_return_if_fail (MCP_IS_SERVER (self));
    g_return_if_fail (messages != NULL);

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_server_request_sampling_async);

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Server not ready");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("sampling/createMessage", id);

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    /* messages array */
    json_builder_set_member_name (builder, "messages");
    json_builder_begin_array (builder);
    for (l = messages; l != NULL; l = l->next)
    {
        McpSamplingMessage *msg = l->data;
        g_autoptr(JsonNode) msg_node = mcp_sampling_message_to_json (msg);
        json_builder_add_value (builder, g_steal_pointer (&msg_node));
    }
    json_builder_end_array (builder);

    /* optional model preferences */
    if (model_preferences != NULL)
    {
        g_autoptr(JsonNode) prefs_node = mcp_model_preferences_to_json (model_preferences);
        json_builder_set_member_name (builder, "modelPreferences");
        json_builder_add_value (builder, g_steal_pointer (&prefs_node));
    }

    /* optional system prompt */
    if (system_prompt != NULL)
    {
        json_builder_set_member_name (builder, "systemPrompt");
        json_builder_add_string_value (builder, system_prompt);
    }

    /* optional max tokens */
    if (max_tokens > 0)
    {
        json_builder_set_member_name (builder, "maxTokens");
        json_builder_add_int_value (builder, max_tokens);
    }

    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    mcp_request_set_params (request, g_steal_pointer (&params));

    send_request (self, request, task);
    g_object_unref (task);
}

/**
 * mcp_server_request_sampling_finish:
 * @self: an #McpServer
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous sampling request.
 *
 * Returns: (transfer full) (nullable): the #McpSamplingResult, or %NULL on error
 */
McpSamplingResult *
mcp_server_request_sampling_finish (McpServer     *self,
                                    GAsyncResult  *result,
                                    GError       **error)
{
    JsonNode *node;
    McpSamplingResult *sampling_result;

    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    node = g_task_propagate_pointer (G_TASK (result), error);
    if (node == NULL)
    {
        return NULL;
    }

    sampling_result = mcp_sampling_result_new_from_json (node, error);
    json_node_unref (node);

    return sampling_result;
}

/* ========================================================================== */
/* Roots Request API                                                          */
/* ========================================================================== */

/**
 * mcp_server_list_roots_async:
 * @self: an #McpServer
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): callback to call when complete
 * @user_data: (closure): user data for @callback
 *
 * Requests the list of roots from the client.
 */
void
mcp_server_list_roots_async (McpServer           *self,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    GTask *task;
    g_autoptr(McpRequest) request = NULL;
    g_autofree gchar *id = NULL;

    g_return_if_fail (MCP_IS_SERVER (self));

    task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_source_tag (task, mcp_server_list_roots_async);

    if (mcp_session_get_state (MCP_SESSION (self)) != MCP_SESSION_STATE_READY)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_INTERNAL_ERROR,
                                 "Server not ready");
        g_object_unref (task);
        return;
    }

    id = mcp_session_generate_request_id (MCP_SESSION (self));
    request = mcp_request_new ("roots/list", id);

    send_request (self, request, task);
    g_object_unref (task);
}

/**
 * mcp_server_list_roots_finish:
 * @self: an #McpServer
 * @result: the #GAsyncResult
 * @error: (nullable): return location for a #GError
 *
 * Completes an asynchronous roots list request.
 *
 * Returns: (transfer full) (element-type McpRoot): list of roots, or %NULL on error
 */
GList *
mcp_server_list_roots_finish (McpServer     *self,
                              GAsyncResult  *result,
                              GError       **error)
{
    JsonNode *node;
    JsonObject *obj;
    JsonArray *roots_arr;
    GList *roots = NULL;
    guint i;
    guint len;

    g_return_val_if_fail (MCP_IS_SERVER (self), NULL);
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    node = g_task_propagate_pointer (G_TASK (result), error);
    if (node == NULL)
    {
        return NULL;
    }

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Invalid roots response");
        json_node_unref (node);
        return NULL;
    }

    obj = json_node_get_object (node);
    if (!json_object_has_member (obj, "roots"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Missing 'roots' field");
        json_node_unref (node);
        return NULL;
    }

    roots_arr = json_object_get_array_member (obj, "roots");
    len = json_array_get_length (roots_arr);

    for (i = 0; i < len; i++)
    {
        JsonNode *root_node = json_array_get_element (roots_arr, i);
        g_autoptr(GError) parse_error = NULL;
        McpRoot *root;

        root = mcp_root_new_from_json (root_node, &parse_error);
        if (root != NULL)
        {
            roots = g_list_append (roots, root);
        }
        else
        {
            g_warning ("Failed to parse root: %s", parse_error->message);
        }
    }

    json_node_unref (node);
    return roots;
}

/* ========================================================================== */
/* Completion Handler API                                                     */
/* ========================================================================== */

/**
 * mcp_server_set_completion_handler:
 * @self: an #McpServer
 * @handler: (scope notified) (nullable): the handler function
 * @user_data: (closure): user data for @handler
 * @destroy: (destroy user_data): destroy notify for @user_data
 *
 * Sets the completion handler for autocomplete requests.
 */
void
mcp_server_set_completion_handler (McpServer            *self,
                                   McpCompletionHandler  handler,
                                   gpointer              user_data,
                                   GDestroyNotify        destroy)
{
    g_return_if_fail (MCP_IS_SERVER (self));

    /* Clean up old handler */
    if (self->completion_destroy != NULL && self->completion_user_data != NULL)
    {
        self->completion_destroy (self->completion_user_data);
    }

    self->completion_handler = handler;
    self->completion_user_data = user_data;
    self->completion_destroy = destroy;
}
