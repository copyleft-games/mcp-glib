/*
 * mcp-message.c - JSON-RPC 2.0 message types implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define MCP_COMPILATION
#include <mcp/mcp-message.h>
#include <mcp/mcp-enums.h>
#include <mcp/mcp-error.h>
#undef MCP_COMPILATION

/* ========================================================================== */
/* McpMessage - Abstract base class                                           */
/* ========================================================================== */

typedef struct
{
    McpMessageType message_type;
} McpMessagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (McpMessage, mcp_message, G_TYPE_OBJECT)

enum
{
    PROP_MESSAGE_0,
    PROP_MESSAGE_TYPE,
    N_MESSAGE_PROPERTIES
};

static GParamSpec *message_properties[N_MESSAGE_PROPERTIES];

static void
mcp_message_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    McpMessage *self = MCP_MESSAGE (object);
    McpMessagePrivate *priv = mcp_message_get_instance_private (self);

    switch (prop_id)
    {
        case PROP_MESSAGE_TYPE:
            g_value_set_enum (value, priv->message_type);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_message_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    switch (prop_id)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

/*
 * Default virtual to_json implementation returns NULL.
 * Subclasses must override this.
 */
static JsonNode *
mcp_message_real_to_json (McpMessage *self)
{
    (void)self;
    return NULL;
}

static void
mcp_message_class_init (McpMessageClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->get_property = mcp_message_get_property;
    object_class->set_property = mcp_message_set_property;

    klass->to_json = mcp_message_real_to_json;

    /**
     * McpMessage:message-type:
     *
     * The type of this message (request, response, error, notification).
     * This property is read-only and is set automatically by the subclass.
     */
    message_properties[PROP_MESSAGE_TYPE] =
        g_param_spec_enum ("message-type",
                           "Message Type",
                           "The type of this message",
                           MCP_TYPE_MESSAGE_TYPE,
                           MCP_MESSAGE_TYPE_REQUEST,
                           G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_MESSAGE_PROPERTIES, message_properties);
}

static void
mcp_message_init (McpMessage *self)
{
    (void)self;
}

/**
 * mcp_message_get_message_type:
 * @self: an #McpMessage
 *
 * Gets the type of the message.
 *
 * Returns: the #McpMessageType
 */
McpMessageType
mcp_message_get_message_type (McpMessage *self)
{
    McpMessagePrivate *priv;

    g_return_val_if_fail (MCP_IS_MESSAGE (self), MCP_MESSAGE_TYPE_REQUEST);

    priv = mcp_message_get_instance_private (self);
    return priv->message_type;
}

/**
 * mcp_message_to_json:
 * @self: an #McpMessage
 *
 * Serializes the message to a JSON node.
 * This calls the virtual to_json method on the subclass.
 *
 * Returns: (transfer full): a #JsonNode containing the message
 */
JsonNode *
mcp_message_to_json (McpMessage *self)
{
    McpMessageClass *klass;

    g_return_val_if_fail (MCP_IS_MESSAGE (self), NULL);

    klass = MCP_MESSAGE_GET_CLASS (self);
    if (klass->to_json != NULL)
    {
        return klass->to_json (self);
    }

    return NULL;
}

/**
 * mcp_message_to_string:
 * @self: an #McpMessage
 *
 * Serializes the message to a JSON string.
 *
 * Returns: (transfer full): a JSON string representation
 */
gchar *
mcp_message_to_string (McpMessage *self)
{
    g_autoptr(JsonNode) node = NULL;
    g_autoptr(JsonGenerator) generator = NULL;

    g_return_val_if_fail (MCP_IS_MESSAGE (self), NULL);

    node = mcp_message_to_json (self);
    if (node == NULL)
    {
        return NULL;
    }

    generator = json_generator_new ();
    json_generator_set_root (generator, node);

    return json_generator_to_data (generator, NULL);
}

/**
 * mcp_message_parse:
 * @json_str: a JSON string to parse
 * @error: (nullable): return location for a #GError
 *
 * Parses a JSON string and creates the appropriate message type.
 *
 * Returns: (transfer full) (nullable): the parsed #McpMessage, or %NULL on error
 */
McpMessage *
mcp_message_parse (const gchar  *json_str,
                   GError      **error)
{
    g_autoptr(JsonParser) parser = NULL;
    JsonNode *root;

    g_return_val_if_fail (json_str != NULL, NULL);

    parser = json_parser_new ();
    if (!json_parser_load_from_data (parser, json_str, -1, error))
    {
        return NULL;
    }

    root = json_parser_get_root (parser);
    return mcp_message_new_from_json (root, error);
}

/**
 * mcp_message_new_from_json:
 * @node: a #JsonNode containing a message
 * @error: (nullable): return location for a #GError
 *
 * Creates the appropriate message type from a JSON node.
 * Determines the message type by examining the JSON structure:
 * - Has "id" and "method": Request
 * - Has "id" and "result": Response
 * - Has "id" and "error": ErrorResponse
 * - Has "method" but no "id": Notification
 *
 * Returns: (transfer full) (nullable): the parsed #McpMessage, or %NULL on error
 */
McpMessage *
mcp_message_new_from_json (JsonNode  *node,
                           GError   **error)
{
    JsonObject *obj;
    gboolean has_id;
    gboolean has_method;
    gboolean has_result;
    gboolean has_error;
    const gchar *jsonrpc;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error,
                     MCP_ERROR,
                     MCP_ERROR_PARSE_ERROR,
                     "Message must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    /* Verify jsonrpc version */
    if (!json_object_has_member (obj, "jsonrpc"))
    {
        g_set_error (error,
                     MCP_ERROR,
                     MCP_ERROR_INVALID_REQUEST,
                     "Missing 'jsonrpc' field");
        return NULL;
    }

    jsonrpc = json_object_get_string_member (obj, "jsonrpc");
    if (g_strcmp0 (jsonrpc, MCP_JSONRPC_VERSION) != 0)
    {
        g_set_error (error,
                     MCP_ERROR,
                     MCP_ERROR_INVALID_REQUEST,
                     "Invalid JSON-RPC version: %s", jsonrpc);
        return NULL;
    }

    /* Determine message type based on structure */
    has_id = json_object_has_member (obj, "id");
    has_method = json_object_has_member (obj, "method");
    has_result = json_object_has_member (obj, "result");
    has_error = json_object_has_member (obj, "error");

    if (has_method && has_id)
    {
        /* Request */
        const gchar *method;
        const gchar *id;
        JsonNode *params_node;
        McpRequest *request;

        method = json_object_get_string_member (obj, "method");
        id = json_object_get_string_member (obj, "id");

        params_node = json_object_has_member (obj, "params") ?
                      json_object_get_member (obj, "params") : NULL;

        if (params_node != NULL)
        {
            request = mcp_request_new_with_params (method, id, json_node_copy (params_node));
        }
        else
        {
            request = mcp_request_new (method, id);
        }

        return MCP_MESSAGE (request);
    }
    else if (has_method && !has_id)
    {
        /* Notification */
        const gchar *method;
        JsonNode *params_node;
        McpNotification *notification;

        method = json_object_get_string_member (obj, "method");
        params_node = json_object_has_member (obj, "params") ?
                      json_object_get_member (obj, "params") : NULL;

        if (params_node != NULL)
        {
            notification = mcp_notification_new_with_params (method, json_node_copy (params_node));
        }
        else
        {
            notification = mcp_notification_new (method);
        }

        return MCP_MESSAGE (notification);
    }
    else if (has_result && has_id)
    {
        /* Success Response */
        const gchar *id;
        JsonNode *result_node;
        McpResponse *response;

        id = json_object_get_string_member (obj, "id");
        result_node = json_object_get_member (obj, "result");

        response = mcp_response_new (id, json_node_copy (result_node));

        return MCP_MESSAGE (response);
    }
    else if (has_error && has_id)
    {
        /* Error Response */
        const gchar *id;
        JsonObject *error_obj;
        gint code;
        const gchar *message;
        JsonNode *data_node;
        McpErrorResponse *err_response;

        id = json_object_get_string_member (obj, "id");
        error_obj = json_object_get_object_member (obj, "error");

        if (error_obj == NULL ||
            !json_object_has_member (error_obj, "code") ||
            !json_object_has_member (error_obj, "message"))
        {
            g_set_error (error,
                         MCP_ERROR,
                         MCP_ERROR_INVALID_REQUEST,
                         "Invalid error object structure");
            return NULL;
        }

        code = (gint)json_object_get_int_member (error_obj, "code");
        message = json_object_get_string_member (error_obj, "message");
        data_node = json_object_has_member (error_obj, "data") ?
                    json_object_get_member (error_obj, "data") : NULL;

        if (data_node != NULL)
        {
            err_response = mcp_error_response_new_with_data (id, code, message,
                                                             json_node_copy (data_node));
        }
        else
        {
            err_response = mcp_error_response_new (id, code, message);
        }

        return MCP_MESSAGE (err_response);
    }
    else if (has_error && !has_id)
    {
        /* Error Response with null id (parse errors) */
        JsonObject *error_obj;
        gint code;
        const gchar *message;
        JsonNode *data_node;
        McpErrorResponse *err_response;

        error_obj = json_object_get_object_member (obj, "error");

        if (error_obj == NULL ||
            !json_object_has_member (error_obj, "code") ||
            !json_object_has_member (error_obj, "message"))
        {
            g_set_error (error,
                         MCP_ERROR,
                         MCP_ERROR_INVALID_REQUEST,
                         "Invalid error object structure");
            return NULL;
        }

        code = (gint)json_object_get_int_member (error_obj, "code");
        message = json_object_get_string_member (error_obj, "message");
        data_node = json_object_has_member (error_obj, "data") ?
                    json_object_get_member (error_obj, "data") : NULL;

        if (data_node != NULL)
        {
            err_response = mcp_error_response_new_with_data (NULL, code, message,
                                                             json_node_copy (data_node));
        }
        else
        {
            err_response = mcp_error_response_new (NULL, code, message);
        }

        return MCP_MESSAGE (err_response);
    }

    g_set_error (error,
                 MCP_ERROR,
                 MCP_ERROR_INVALID_REQUEST,
                 "Cannot determine message type from JSON structure");
    return NULL;
}


/* ========================================================================== */
/* McpRequest                                                                 */
/* ========================================================================== */

struct _McpRequest
{
    McpMessage parent_instance;

    gchar    *id;
    gchar    *method;
    JsonNode *params;
};

G_DEFINE_TYPE (McpRequest, mcp_request, MCP_TYPE_MESSAGE)

enum
{
    PROP_REQUEST_0,
    PROP_REQUEST_ID,
    PROP_REQUEST_METHOD,
    PROP_REQUEST_PARAMS,
    N_REQUEST_PROPERTIES
};

static GParamSpec *request_properties[N_REQUEST_PROPERTIES];

static void
mcp_request_finalize (GObject *object)
{
    McpRequest *self = MCP_REQUEST (object);

    g_clear_pointer (&self->id, g_free);
    g_clear_pointer (&self->method, g_free);
    g_clear_pointer (&self->params, json_node_unref);

    G_OBJECT_CLASS (mcp_request_parent_class)->finalize (object);
}

static void
mcp_request_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    McpRequest *self = MCP_REQUEST (object);

    switch (prop_id)
    {
        case PROP_REQUEST_ID:
            g_value_set_string (value, self->id);
            break;
        case PROP_REQUEST_METHOD:
            g_value_set_string (value, self->method);
            break;
        case PROP_REQUEST_PARAMS:
            g_value_set_boxed (value, self->params);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_request_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    McpRequest *self = MCP_REQUEST (object);

    switch (prop_id)
    {
        case PROP_REQUEST_ID:
            g_free (self->id);
            self->id = g_value_dup_string (value);
            break;
        case PROP_REQUEST_METHOD:
            g_free (self->method);
            self->method = g_value_dup_string (value);
            break;
        case PROP_REQUEST_PARAMS:
            g_clear_pointer (&self->params, json_node_unref);
            self->params = g_value_dup_boxed (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

/*
 * Serializes the request to JSON:
 * {
 *   "jsonrpc": "2.0",
 *   "id": "...",
 *   "method": "...",
 *   "params": { ... }  // optional
 * }
 */
static JsonNode *
mcp_request_to_json (McpMessage *message)
{
    McpRequest *self = MCP_REQUEST (message);
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *node;

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "jsonrpc");
    json_builder_add_string_value (builder, MCP_JSONRPC_VERSION);

    json_builder_set_member_name (builder, "id");
    json_builder_add_string_value (builder, self->id);

    json_builder_set_member_name (builder, "method");
    json_builder_add_string_value (builder, self->method);

    if (self->params != NULL)
    {
        json_builder_set_member_name (builder, "params");
        json_builder_add_value (builder, json_node_copy (self->params));
    }

    json_builder_end_object (builder);

    node = json_builder_get_root (builder);
    return node;
}

static void
mcp_request_class_init (McpRequestClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    McpMessageClass *message_class = MCP_MESSAGE_CLASS (klass);

    object_class->finalize = mcp_request_finalize;
    object_class->get_property = mcp_request_get_property;
    object_class->set_property = mcp_request_set_property;

    message_class->to_json = mcp_request_to_json;

    /**
     * McpRequest:id:
     *
     * The request ID.
     */
    request_properties[PROP_REQUEST_ID] =
        g_param_spec_string ("id",
                             "ID",
                             "The request ID",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT |
                             G_PARAM_STATIC_STRINGS);

    /**
     * McpRequest:method:
     *
     * The method name being called.
     */
    request_properties[PROP_REQUEST_METHOD] =
        g_param_spec_string ("method",
                             "Method",
                             "The method name",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT |
                             G_PARAM_STATIC_STRINGS);

    /**
     * McpRequest:params:
     *
     * The request parameters as a JSON node.
     */
    request_properties[PROP_REQUEST_PARAMS] =
        g_param_spec_boxed ("params",
                            "Params",
                            "The request parameters",
                            JSON_TYPE_NODE,
                            G_PARAM_READWRITE |
                            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_REQUEST_PROPERTIES, request_properties);
}

static void
mcp_request_init (McpRequest *self)
{
    McpMessagePrivate *priv;

    priv = mcp_message_get_instance_private (MCP_MESSAGE (self));
    priv->message_type = MCP_MESSAGE_TYPE_REQUEST;
}

/**
 * mcp_request_new:
 * @method: the method name
 * @id: the request ID
 *
 * Creates a new JSON-RPC request.
 *
 * Returns: (transfer full): a new #McpRequest
 */
McpRequest *
mcp_request_new (const gchar *method,
                 const gchar *id)
{
    g_return_val_if_fail (method != NULL, NULL);
    g_return_val_if_fail (id != NULL, NULL);

    return g_object_new (MCP_TYPE_REQUEST,
                         "method", method,
                         "id", id,
                         NULL);
}

/**
 * mcp_request_new_with_params:
 * @method: the method name
 * @id: the request ID
 * @params: (nullable) (transfer full): the request parameters
 *
 * Creates a new JSON-RPC request with parameters.
 *
 * Returns: (transfer full): a new #McpRequest
 */
McpRequest *
mcp_request_new_with_params (const gchar *method,
                             const gchar *id,
                             JsonNode    *params)
{
    McpRequest *request;

    g_return_val_if_fail (method != NULL, NULL);
    g_return_val_if_fail (id != NULL, NULL);

    request = g_object_new (MCP_TYPE_REQUEST,
                            "method", method,
                            "id", id,
                            NULL);

    if (params != NULL)
    {
        request->params = params;
    }

    return request;
}

/**
 * mcp_request_get_id:
 * @self: an #McpRequest
 *
 * Gets the request ID.
 *
 * Returns: (transfer none): the request ID
 */
const gchar *
mcp_request_get_id (McpRequest *self)
{
    g_return_val_if_fail (MCP_IS_REQUEST (self), NULL);

    return self->id;
}

/**
 * mcp_request_get_method:
 * @self: an #McpRequest
 *
 * Gets the method name.
 *
 * Returns: (transfer none): the method name
 */
const gchar *
mcp_request_get_method (McpRequest *self)
{
    g_return_val_if_fail (MCP_IS_REQUEST (self), NULL);

    return self->method;
}

/**
 * mcp_request_get_params:
 * @self: an #McpRequest
 *
 * Gets the request parameters.
 *
 * Returns: (transfer none) (nullable): the parameters, or %NULL if none
 */
JsonNode *
mcp_request_get_params (McpRequest *self)
{
    g_return_val_if_fail (MCP_IS_REQUEST (self), NULL);

    return self->params;
}

/**
 * mcp_request_set_params:
 * @self: an #McpRequest
 * @params: (nullable) (transfer full): the parameters
 *
 * Sets the request parameters.
 */
void
mcp_request_set_params (McpRequest *self,
                        JsonNode   *params)
{
    g_return_if_fail (MCP_IS_REQUEST (self));

    g_clear_pointer (&self->params, json_node_unref);
    self->params = params;
}


/* ========================================================================== */
/* McpResponse                                                                */
/* ========================================================================== */

struct _McpResponse
{
    McpMessage parent_instance;

    gchar    *id;
    JsonNode *result;
};

G_DEFINE_TYPE (McpResponse, mcp_response, MCP_TYPE_MESSAGE)

enum
{
    PROP_RESPONSE_0,
    PROP_RESPONSE_ID,
    PROP_RESPONSE_RESULT,
    N_RESPONSE_PROPERTIES
};

static GParamSpec *response_properties[N_RESPONSE_PROPERTIES];

static void
mcp_response_finalize (GObject *object)
{
    McpResponse *self = MCP_RESPONSE (object);

    g_clear_pointer (&self->id, g_free);
    g_clear_pointer (&self->result, json_node_unref);

    G_OBJECT_CLASS (mcp_response_parent_class)->finalize (object);
}

static void
mcp_response_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    McpResponse *self = MCP_RESPONSE (object);

    switch (prop_id)
    {
        case PROP_RESPONSE_ID:
            g_value_set_string (value, self->id);
            break;
        case PROP_RESPONSE_RESULT:
            g_value_set_boxed (value, self->result);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_response_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    McpResponse *self = MCP_RESPONSE (object);

    switch (prop_id)
    {
        case PROP_RESPONSE_ID:
            g_free (self->id);
            self->id = g_value_dup_string (value);
            break;
        case PROP_RESPONSE_RESULT:
            g_clear_pointer (&self->result, json_node_unref);
            self->result = g_value_dup_boxed (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

/*
 * Serializes the response to JSON:
 * {
 *   "jsonrpc": "2.0",
 *   "id": "...",
 *   "result": { ... }
 * }
 */
static JsonNode *
mcp_response_to_json (McpMessage *message)
{
    McpResponse *self = MCP_RESPONSE (message);
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *node;

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "jsonrpc");
    json_builder_add_string_value (builder, MCP_JSONRPC_VERSION);

    json_builder_set_member_name (builder, "id");
    json_builder_add_string_value (builder, self->id);

    json_builder_set_member_name (builder, "result");
    if (self->result != NULL)
    {
        json_builder_add_value (builder, json_node_copy (self->result));
    }
    else
    {
        json_builder_add_null_value (builder);
    }

    json_builder_end_object (builder);

    node = json_builder_get_root (builder);
    return node;
}

static void
mcp_response_class_init (McpResponseClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    McpMessageClass *message_class = MCP_MESSAGE_CLASS (klass);

    object_class->finalize = mcp_response_finalize;
    object_class->get_property = mcp_response_get_property;
    object_class->set_property = mcp_response_set_property;

    message_class->to_json = mcp_response_to_json;

    /**
     * McpResponse:id:
     *
     * The response ID (matches the request ID).
     */
    response_properties[PROP_RESPONSE_ID] =
        g_param_spec_string ("id",
                             "ID",
                             "The response ID",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT |
                             G_PARAM_STATIC_STRINGS);

    /**
     * McpResponse:result:
     *
     * The result as a JSON node.
     */
    response_properties[PROP_RESPONSE_RESULT] =
        g_param_spec_boxed ("result",
                            "Result",
                            "The result",
                            JSON_TYPE_NODE,
                            G_PARAM_READWRITE |
                            G_PARAM_CONSTRUCT |
                            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_RESPONSE_PROPERTIES, response_properties);
}

static void
mcp_response_init (McpResponse *self)
{
    McpMessagePrivate *priv;

    priv = mcp_message_get_instance_private (MCP_MESSAGE (self));
    priv->message_type = MCP_MESSAGE_TYPE_RESPONSE;
}

/**
 * mcp_response_new:
 * @id: the request ID this is responding to
 * @result: (transfer full): the result as a #JsonNode
 *
 * Creates a new JSON-RPC success response.
 *
 * Returns: (transfer full): a new #McpResponse
 */
McpResponse *
mcp_response_new (const gchar *id,
                  JsonNode    *result)
{
    McpResponse *response;

    g_return_val_if_fail (id != NULL, NULL);

    response = g_object_new (MCP_TYPE_RESPONSE,
                             "id", id,
                             NULL);

    response->result = result;

    return response;
}

/**
 * mcp_response_get_id:
 * @self: an #McpResponse
 *
 * Gets the response ID.
 *
 * Returns: (transfer none): the response ID
 */
const gchar *
mcp_response_get_id (McpResponse *self)
{
    g_return_val_if_fail (MCP_IS_RESPONSE (self), NULL);

    return self->id;
}

/**
 * mcp_response_get_result:
 * @self: an #McpResponse
 *
 * Gets the result.
 *
 * Returns: (transfer none): the result
 */
JsonNode *
mcp_response_get_result (McpResponse *self)
{
    g_return_val_if_fail (MCP_IS_RESPONSE (self), NULL);

    return self->result;
}


/* ========================================================================== */
/* McpErrorResponse                                                           */
/* ========================================================================== */

struct _McpErrorResponse
{
    McpMessage parent_instance;

    gchar    *id;
    gint      code;
    gchar    *message;
    JsonNode *data;
};

G_DEFINE_TYPE (McpErrorResponse, mcp_error_response, MCP_TYPE_MESSAGE)

enum
{
    PROP_ERROR_0,
    PROP_ERROR_ID,
    PROP_ERROR_CODE,
    PROP_ERROR_MESSAGE,
    PROP_ERROR_DATA,
    N_ERROR_PROPERTIES
};

static GParamSpec *error_properties[N_ERROR_PROPERTIES];

static void
mcp_error_response_finalize (GObject *object)
{
    McpErrorResponse *self = MCP_ERROR_RESPONSE (object);

    g_clear_pointer (&self->id, g_free);
    g_clear_pointer (&self->message, g_free);
    g_clear_pointer (&self->data, json_node_unref);

    G_OBJECT_CLASS (mcp_error_response_parent_class)->finalize (object);
}

static void
mcp_error_response_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    McpErrorResponse *self = MCP_ERROR_RESPONSE (object);

    switch (prop_id)
    {
        case PROP_ERROR_ID:
            g_value_set_string (value, self->id);
            break;
        case PROP_ERROR_CODE:
            g_value_set_int (value, self->code);
            break;
        case PROP_ERROR_MESSAGE:
            g_value_set_string (value, self->message);
            break;
        case PROP_ERROR_DATA:
            g_value_set_boxed (value, self->data);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_error_response_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    McpErrorResponse *self = MCP_ERROR_RESPONSE (object);

    switch (prop_id)
    {
        case PROP_ERROR_ID:
            g_free (self->id);
            self->id = g_value_dup_string (value);
            break;
        case PROP_ERROR_CODE:
            self->code = g_value_get_int (value);
            break;
        case PROP_ERROR_MESSAGE:
            g_free (self->message);
            self->message = g_value_dup_string (value);
            break;
        case PROP_ERROR_DATA:
            g_clear_pointer (&self->data, json_node_unref);
            self->data = g_value_dup_boxed (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

/*
 * Serializes the error response to JSON:
 * {
 *   "jsonrpc": "2.0",
 *   "id": "..." | null,
 *   "error": {
 *     "code": -32xxx,
 *     "message": "...",
 *     "data": { ... }  // optional
 *   }
 * }
 */
static JsonNode *
mcp_error_response_to_json (McpMessage *message)
{
    McpErrorResponse *self = MCP_ERROR_RESPONSE (message);
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *node;

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "jsonrpc");
    json_builder_add_string_value (builder, MCP_JSONRPC_VERSION);

    json_builder_set_member_name (builder, "id");
    if (self->id != NULL)
    {
        json_builder_add_string_value (builder, self->id);
    }
    else
    {
        json_builder_add_null_value (builder);
    }

    json_builder_set_member_name (builder, "error");
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "code");
    json_builder_add_int_value (builder, self->code);

    json_builder_set_member_name (builder, "message");
    json_builder_add_string_value (builder, self->message);

    if (self->data != NULL)
    {
        json_builder_set_member_name (builder, "data");
        json_builder_add_value (builder, json_node_copy (self->data));
    }

    json_builder_end_object (builder);  /* error object */

    json_builder_end_object (builder);  /* root object */

    node = json_builder_get_root (builder);
    return node;
}

static void
mcp_error_response_class_init (McpErrorResponseClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    McpMessageClass *message_class = MCP_MESSAGE_CLASS (klass);

    object_class->finalize = mcp_error_response_finalize;
    object_class->get_property = mcp_error_response_get_property;
    object_class->set_property = mcp_error_response_set_property;

    message_class->to_json = mcp_error_response_to_json;

    /**
     * McpErrorResponse:id:
     *
     * The response ID (can be NULL for parse errors).
     */
    error_properties[PROP_ERROR_ID] =
        g_param_spec_string ("id",
                             "ID",
                             "The response ID",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT |
                             G_PARAM_STATIC_STRINGS);

    /**
     * McpErrorResponse:code:
     *
     * The error code.
     */
    error_properties[PROP_ERROR_CODE] =
        g_param_spec_int ("code",
                          "Code",
                          "The error code",
                          G_MININT,
                          G_MAXINT,
                          MCP_ERROR_INTERNAL_ERROR,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS);

    /**
     * McpErrorResponse:message:
     *
     * The error message.
     */
    error_properties[PROP_ERROR_MESSAGE] =
        g_param_spec_string ("message",
                             "Message",
                             "The error message",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT |
                             G_PARAM_STATIC_STRINGS);

    /**
     * McpErrorResponse:data:
     *
     * Additional error data.
     */
    error_properties[PROP_ERROR_DATA] =
        g_param_spec_boxed ("data",
                            "Data",
                            "Additional error data",
                            JSON_TYPE_NODE,
                            G_PARAM_READWRITE |
                            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_ERROR_PROPERTIES, error_properties);
}

static void
mcp_error_response_init (McpErrorResponse *self)
{
    McpMessagePrivate *priv;

    priv = mcp_message_get_instance_private (MCP_MESSAGE (self));
    priv->message_type = MCP_MESSAGE_TYPE_ERROR;
}

/**
 * mcp_error_response_new:
 * @id: (nullable): the request ID this is responding to
 * @code: the error code
 * @message: the error message
 *
 * Creates a new JSON-RPC error response.
 *
 * Returns: (transfer full): a new #McpErrorResponse
 */
McpErrorResponse *
mcp_error_response_new (const gchar *id,
                        gint         code,
                        const gchar *message)
{
    g_return_val_if_fail (message != NULL, NULL);

    return g_object_new (MCP_TYPE_ERROR_RESPONSE,
                         "id", id,
                         "code", code,
                         "message", message,
                         NULL);
}

/**
 * mcp_error_response_new_with_data:
 * @id: (nullable): the request ID
 * @code: the error code
 * @message: the error message
 * @data: (nullable) (transfer full): additional error data
 *
 * Creates a new JSON-RPC error response with additional data.
 *
 * Returns: (transfer full): a new #McpErrorResponse
 */
McpErrorResponse *
mcp_error_response_new_with_data (const gchar *id,
                                  gint         code,
                                  const gchar *message,
                                  JsonNode    *data)
{
    McpErrorResponse *err;

    g_return_val_if_fail (message != NULL, NULL);

    err = g_object_new (MCP_TYPE_ERROR_RESPONSE,
                        "id", id,
                        "code", code,
                        "message", message,
                        NULL);

    if (data != NULL)
    {
        err->data = data;
    }

    return err;
}

/**
 * mcp_error_response_new_from_gerror:
 * @id: (nullable): the request ID
 * @error: a #GError
 *
 * Creates a new JSON-RPC error response from a GError.
 * Uses MCP_ERROR_INTERNAL_ERROR if the error is not in the MCP domain.
 *
 * Returns: (transfer full): a new #McpErrorResponse
 */
McpErrorResponse *
mcp_error_response_new_from_gerror (const gchar  *id,
                                    const GError *error)
{
    gint code;

    g_return_val_if_fail (error != NULL, NULL);

    if (error->domain == MCP_ERROR)
    {
        code = error->code;
    }
    else
    {
        code = MCP_ERROR_INTERNAL_ERROR;
    }

    return mcp_error_response_new (id, code, error->message);
}

/**
 * mcp_error_response_get_id:
 * @self: an #McpErrorResponse
 *
 * Gets the response ID.
 *
 * Returns: (transfer none) (nullable): the response ID, or %NULL for parse errors
 */
const gchar *
mcp_error_response_get_id (McpErrorResponse *self)
{
    g_return_val_if_fail (MCP_IS_ERROR_RESPONSE (self), NULL);

    return self->id;
}

/**
 * mcp_error_response_get_code:
 * @self: an #McpErrorResponse
 *
 * Gets the error code.
 *
 * Returns: the error code
 */
gint
mcp_error_response_get_code (McpErrorResponse *self)
{
    g_return_val_if_fail (MCP_IS_ERROR_RESPONSE (self), 0);

    return self->code;
}

/**
 * mcp_error_response_get_error_message:
 * @self: an #McpErrorResponse
 *
 * Gets the error message.
 *
 * Returns: (transfer none): the error message
 */
const gchar *
mcp_error_response_get_error_message (McpErrorResponse *self)
{
    g_return_val_if_fail (MCP_IS_ERROR_RESPONSE (self), NULL);

    return self->message;
}

/**
 * mcp_error_response_get_data:
 * @self: an #McpErrorResponse
 *
 * Gets the additional error data.
 *
 * Returns: (transfer none) (nullable): the error data, or %NULL if none
 */
JsonNode *
mcp_error_response_get_data (McpErrorResponse *self)
{
    g_return_val_if_fail (MCP_IS_ERROR_RESPONSE (self), NULL);

    return self->data;
}

/**
 * mcp_error_response_set_data:
 * @self: an #McpErrorResponse
 * @data: (nullable): the error data
 *
 * Sets the additional error data.
 */
void
mcp_error_response_set_data (McpErrorResponse *self,
                             JsonNode         *data)
{
    g_return_if_fail (MCP_IS_ERROR_RESPONSE (self));

    if (self->data != NULL)
    {
        json_node_unref (self->data);
    }
    self->data = (data != NULL) ? json_node_copy (data) : NULL;
}

/**
 * mcp_error_response_to_gerror:
 * @self: an #McpErrorResponse
 *
 * Converts the error response to a GError.
 *
 * Returns: (transfer full): a new #GError
 */
GError *
mcp_error_response_to_gerror (McpErrorResponse *self)
{
    g_return_val_if_fail (MCP_IS_ERROR_RESPONSE (self), NULL);

    return g_error_new (MCP_ERROR,
                        self->code,
                        "%s",
                        self->message);
}


/* ========================================================================== */
/* McpNotification                                                            */
/* ========================================================================== */

struct _McpNotification
{
    McpMessage parent_instance;

    gchar    *method;
    JsonNode *params;
};

G_DEFINE_TYPE (McpNotification, mcp_notification, MCP_TYPE_MESSAGE)

enum
{
    PROP_NOTIFICATION_0,
    PROP_NOTIFICATION_METHOD,
    PROP_NOTIFICATION_PARAMS,
    N_NOTIFICATION_PROPERTIES
};

static GParamSpec *notification_properties[N_NOTIFICATION_PROPERTIES];

static void
mcp_notification_finalize (GObject *object)
{
    McpNotification *self = MCP_NOTIFICATION (object);

    g_clear_pointer (&self->method, g_free);
    g_clear_pointer (&self->params, json_node_unref);

    G_OBJECT_CLASS (mcp_notification_parent_class)->finalize (object);
}

static void
mcp_notification_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
    McpNotification *self = MCP_NOTIFICATION (object);

    switch (prop_id)
    {
        case PROP_NOTIFICATION_METHOD:
            g_value_set_string (value, self->method);
            break;
        case PROP_NOTIFICATION_PARAMS:
            g_value_set_boxed (value, self->params);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_notification_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    McpNotification *self = MCP_NOTIFICATION (object);

    switch (prop_id)
    {
        case PROP_NOTIFICATION_METHOD:
            g_free (self->method);
            self->method = g_value_dup_string (value);
            break;
        case PROP_NOTIFICATION_PARAMS:
            g_clear_pointer (&self->params, json_node_unref);
            self->params = g_value_dup_boxed (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

/*
 * Serializes the notification to JSON:
 * {
 *   "jsonrpc": "2.0",
 *   "method": "...",
 *   "params": { ... }  // optional
 * }
 * Note: No "id" field - that's what distinguishes notifications from requests
 */
static JsonNode *
mcp_notification_to_json (McpMessage *message)
{
    McpNotification *self = MCP_NOTIFICATION (message);
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *node;

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "jsonrpc");
    json_builder_add_string_value (builder, MCP_JSONRPC_VERSION);

    json_builder_set_member_name (builder, "method");
    json_builder_add_string_value (builder, self->method);

    if (self->params != NULL)
    {
        json_builder_set_member_name (builder, "params");
        json_builder_add_value (builder, json_node_copy (self->params));
    }

    json_builder_end_object (builder);

    node = json_builder_get_root (builder);
    return node;
}

static void
mcp_notification_class_init (McpNotificationClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    McpMessageClass *message_class = MCP_MESSAGE_CLASS (klass);

    object_class->finalize = mcp_notification_finalize;
    object_class->get_property = mcp_notification_get_property;
    object_class->set_property = mcp_notification_set_property;

    message_class->to_json = mcp_notification_to_json;

    /**
     * McpNotification:method:
     *
     * The notification method name.
     */
    notification_properties[PROP_NOTIFICATION_METHOD] =
        g_param_spec_string ("method",
                             "Method",
                             "The notification method name",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT |
                             G_PARAM_STATIC_STRINGS);

    /**
     * McpNotification:params:
     *
     * The notification parameters.
     */
    notification_properties[PROP_NOTIFICATION_PARAMS] =
        g_param_spec_boxed ("params",
                            "Params",
                            "The notification parameters",
                            JSON_TYPE_NODE,
                            G_PARAM_READWRITE |
                            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_NOTIFICATION_PROPERTIES, notification_properties);
}

static void
mcp_notification_init (McpNotification *self)
{
    McpMessagePrivate *priv;

    priv = mcp_message_get_instance_private (MCP_MESSAGE (self));
    priv->message_type = MCP_MESSAGE_TYPE_NOTIFICATION;
}

/**
 * mcp_notification_new:
 * @method: the method name
 *
 * Creates a new JSON-RPC notification.
 *
 * Returns: (transfer full): a new #McpNotification
 */
McpNotification *
mcp_notification_new (const gchar *method)
{
    g_return_val_if_fail (method != NULL, NULL);

    return g_object_new (MCP_TYPE_NOTIFICATION,
                         "method", method,
                         NULL);
}

/**
 * mcp_notification_new_with_params:
 * @method: the method name
 * @params: (nullable) (transfer full): the notification parameters
 *
 * Creates a new JSON-RPC notification with parameters.
 *
 * Returns: (transfer full): a new #McpNotification
 */
McpNotification *
mcp_notification_new_with_params (const gchar *method,
                                  JsonNode    *params)
{
    McpNotification *notification;

    g_return_val_if_fail (method != NULL, NULL);

    notification = g_object_new (MCP_TYPE_NOTIFICATION,
                                 "method", method,
                                 NULL);

    if (params != NULL)
    {
        notification->params = params;
    }

    return notification;
}

/**
 * mcp_notification_get_method:
 * @self: an #McpNotification
 *
 * Gets the method name.
 *
 * Returns: (transfer none): the method name
 */
const gchar *
mcp_notification_get_method (McpNotification *self)
{
    g_return_val_if_fail (MCP_IS_NOTIFICATION (self), NULL);

    return self->method;
}

/**
 * mcp_notification_get_params:
 * @self: an #McpNotification
 *
 * Gets the notification parameters.
 *
 * Returns: (transfer none) (nullable): the parameters, or %NULL if none
 */
JsonNode *
mcp_notification_get_params (McpNotification *self)
{
    g_return_val_if_fail (MCP_IS_NOTIFICATION (self), NULL);

    return self->params;
}

/**
 * mcp_notification_set_params:
 * @self: an #McpNotification
 * @params: (nullable) (transfer full): the parameters
 *
 * Sets the notification parameters.
 */
void
mcp_notification_set_params (McpNotification *self,
                             JsonNode        *params)
{
    g_return_if_fail (MCP_IS_NOTIFICATION (self));

    g_clear_pointer (&self->params, json_node_unref);
    self->params = params;
}
