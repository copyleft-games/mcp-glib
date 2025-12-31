/*
 * mcp-sampling.c - Sampling types implementation for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define MCP_COMPILATION
#include <mcp/mcp.h>

/* ========================================================================== */
/* McpSamplingMessage                                                         */
/* ========================================================================== */

struct _McpSamplingMessage
{
    gint       ref_count;
    McpRole    role;
    JsonArray *content;
};

/**
 * mcp_sampling_message_new:
 * @role: the message role
 *
 * Creates a new sampling message.
 *
 * Returns: (transfer full): a new #McpSamplingMessage
 */
McpSamplingMessage *
mcp_sampling_message_new (McpRole role)
{
    McpSamplingMessage *message;

    message = g_new0 (McpSamplingMessage, 1);
    message->ref_count = 1;
    message->role = role;
    message->content = json_array_new ();

    return message;
}

/**
 * mcp_sampling_message_ref:
 * @message: a #McpSamplingMessage
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpSamplingMessage
 */
McpSamplingMessage *
mcp_sampling_message_ref (McpSamplingMessage *message)
{
    g_return_val_if_fail (message != NULL, NULL);

    message->ref_count++;
    return message;
}

/**
 * mcp_sampling_message_unref:
 * @message: a #McpSamplingMessage
 *
 * Decrements the reference count.
 */
void
mcp_sampling_message_unref (McpSamplingMessage *message)
{
    g_return_if_fail (message != NULL);

    if (--message->ref_count == 0)
    {
        json_array_unref (message->content);
        g_free (message);
    }
}

/**
 * mcp_sampling_message_get_role:
 * @message: a #McpSamplingMessage
 *
 * Gets the message role.
 *
 * Returns: the #McpRole
 */
McpRole
mcp_sampling_message_get_role (McpSamplingMessage *message)
{
    g_return_val_if_fail (message != NULL, MCP_ROLE_USER);
    return message->role;
}

/**
 * mcp_sampling_message_add_text:
 * @message: a #McpSamplingMessage
 * @text: the text content
 *
 * Adds a text content item to the message.
 */
void
mcp_sampling_message_add_text (McpSamplingMessage *message,
                               const gchar        *text)
{
    JsonObject *item;

    g_return_if_fail (message != NULL);
    g_return_if_fail (text != NULL);

    item = json_object_new ();
    json_object_set_string_member (item, "type", "text");
    json_object_set_string_member (item, "text", text);
    json_array_add_object_element (message->content, item);
}

/**
 * mcp_sampling_message_add_image:
 * @message: a #McpSamplingMessage
 * @data: the base64-encoded image data
 * @mime_type: the MIME type
 *
 * Adds an image content item to the message.
 */
void
mcp_sampling_message_add_image (McpSamplingMessage *message,
                                const gchar        *data,
                                const gchar        *mime_type)
{
    JsonObject *item;

    g_return_if_fail (message != NULL);
    g_return_if_fail (data != NULL);
    g_return_if_fail (mime_type != NULL);

    item = json_object_new ();
    json_object_set_string_member (item, "type", "image");
    json_object_set_string_member (item, "data", data);
    json_object_set_string_member (item, "mimeType", mime_type);
    json_array_add_object_element (message->content, item);
}

/**
 * mcp_sampling_message_get_content:
 * @message: a #McpSamplingMessage
 *
 * Gets the content items as a JSON array.
 *
 * Returns: (transfer none): the content array
 */
JsonArray *
mcp_sampling_message_get_content (McpSamplingMessage *message)
{
    g_return_val_if_fail (message != NULL, NULL);
    return message->content;
}

/**
 * mcp_sampling_message_to_json:
 * @message: a #McpSamplingMessage
 *
 * Serializes the message to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *
mcp_sampling_message_to_json (McpSamplingMessage *message)
{
    JsonBuilder *builder;
    JsonNode *node;
    const gchar *role_str;

    g_return_val_if_fail (message != NULL, NULL);

    role_str = mcp_role_to_string (message->role);

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "role");
    json_builder_add_string_value (builder, role_str);

    json_builder_set_member_name (builder, "content");
    {
        JsonNode *arr = json_node_new (JSON_NODE_ARRAY);
        json_node_set_array (arr, message->content);
        json_builder_add_value (builder, arr);
    }

    json_builder_end_object (builder);
    node = json_builder_get_root (builder);
    g_object_unref (builder);

    return node;
}

/**
 * mcp_sampling_message_new_from_json:
 * @node: a #JsonNode containing a message
 * @error: (nullable): return location for a #GError
 *
 * Creates a new sampling message from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpSamplingMessage, or %NULL on error
 */
McpSamplingMessage *
mcp_sampling_message_new_from_json (JsonNode  *node,
                                    GError   **error)
{
    JsonObject *obj;
    McpSamplingMessage *message;
    const gchar *role_str;
    McpRole role;
    JsonArray *content;
    guint i;
    guint len;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Sampling message must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    /* Parse role */
    if (!json_object_has_member (obj, "role"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Sampling message missing required 'role' field");
        return NULL;
    }
    role_str = json_object_get_string_member (obj, "role");
    role = mcp_role_from_string (role_str);

    message = mcp_sampling_message_new (role);

    /* Parse content array */
    if (json_object_has_member (obj, "content"))
    {
        content = json_object_get_array_member (obj, "content");
        len = json_array_get_length (content);
        for (i = 0; i < len; i++)
        {
            JsonObject *item = json_array_get_object_element (content, i);
            const gchar *type;

            if (!json_object_has_member (item, "type"))
            {
                continue;
            }

            type = json_object_get_string_member (item, "type");
            if (g_strcmp0 (type, "text") == 0)
            {
                if (json_object_has_member (item, "text"))
                {
                    mcp_sampling_message_add_text (message,
                        json_object_get_string_member (item, "text"));
                }
            }
            else if (g_strcmp0 (type, "image") == 0)
            {
                if (json_object_has_member (item, "data") &&
                    json_object_has_member (item, "mimeType"))
                {
                    mcp_sampling_message_add_image (message,
                        json_object_get_string_member (item, "data"),
                        json_object_get_string_member (item, "mimeType"));
                }
            }
        }
    }

    return message;
}

/* ========================================================================== */
/* McpModelPreferences                                                        */
/* ========================================================================== */

struct _McpModelPreferences
{
    gint     ref_count;
    GList   *hints;              /* List of model name hints (gchar *) */
    gdouble  cost_priority;      /* 0.0 to 1.0 */
    gdouble  speed_priority;     /* 0.0 to 1.0 */
    gdouble  intelligence_priority; /* 0.0 to 1.0 */
};

/**
 * mcp_model_preferences_new:
 *
 * Creates a new model preferences object with default values.
 *
 * Returns: (transfer full): a new #McpModelPreferences
 */
McpModelPreferences *
mcp_model_preferences_new (void)
{
    McpModelPreferences *prefs;

    prefs = g_new0 (McpModelPreferences, 1);
    prefs->ref_count = 1;
    prefs->hints = NULL;
    prefs->cost_priority = 0.0;
    prefs->speed_priority = 0.0;
    prefs->intelligence_priority = 0.0;

    return prefs;
}

/**
 * mcp_model_preferences_ref:
 * @prefs: a #McpModelPreferences
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpModelPreferences
 */
McpModelPreferences *
mcp_model_preferences_ref (McpModelPreferences *prefs)
{
    g_return_val_if_fail (prefs != NULL, NULL);

    prefs->ref_count++;
    return prefs;
}

/**
 * mcp_model_preferences_unref:
 * @prefs: a #McpModelPreferences
 *
 * Decrements the reference count.
 */
void
mcp_model_preferences_unref (McpModelPreferences *prefs)
{
    g_return_if_fail (prefs != NULL);

    if (--prefs->ref_count == 0)
    {
        g_list_free_full (prefs->hints, g_free);
        g_free (prefs);
    }
}

/**
 * mcp_model_preferences_add_hint:
 * @prefs: a #McpModelPreferences
 * @name: the model name hint
 *
 * Adds a model name hint.
 */
void
mcp_model_preferences_add_hint (McpModelPreferences *prefs,
                                const gchar         *name)
{
    g_return_if_fail (prefs != NULL);
    g_return_if_fail (name != NULL);

    prefs->hints = g_list_append (prefs->hints, g_strdup (name));
}

/**
 * mcp_model_preferences_get_hints:
 * @prefs: a #McpModelPreferences
 *
 * Gets the list of model hints.
 *
 * Returns: (transfer none) (element-type utf8): the hints list
 */
GList *
mcp_model_preferences_get_hints (McpModelPreferences *prefs)
{
    g_return_val_if_fail (prefs != NULL, NULL);
    return prefs->hints;
}

/**
 * mcp_model_preferences_set_cost_priority:
 * @prefs: a #McpModelPreferences
 * @priority: the priority (0.0 to 1.0)
 *
 * Sets the cost priority.
 */
void
mcp_model_preferences_set_cost_priority (McpModelPreferences *prefs,
                                         gdouble              priority)
{
    g_return_if_fail (prefs != NULL);
    prefs->cost_priority = CLAMP (priority, 0.0, 1.0);
}

/**
 * mcp_model_preferences_get_cost_priority:
 * @prefs: a #McpModelPreferences
 *
 * Gets the cost priority.
 *
 * Returns: the cost priority
 */
gdouble
mcp_model_preferences_get_cost_priority (McpModelPreferences *prefs)
{
    g_return_val_if_fail (prefs != NULL, 0.0);
    return prefs->cost_priority;
}

/**
 * mcp_model_preferences_set_speed_priority:
 * @prefs: a #McpModelPreferences
 * @priority: the priority (0.0 to 1.0)
 *
 * Sets the speed priority.
 */
void
mcp_model_preferences_set_speed_priority (McpModelPreferences *prefs,
                                          gdouble              priority)
{
    g_return_if_fail (prefs != NULL);
    prefs->speed_priority = CLAMP (priority, 0.0, 1.0);
}

/**
 * mcp_model_preferences_get_speed_priority:
 * @prefs: a #McpModelPreferences
 *
 * Gets the speed priority.
 *
 * Returns: the speed priority
 */
gdouble
mcp_model_preferences_get_speed_priority (McpModelPreferences *prefs)
{
    g_return_val_if_fail (prefs != NULL, 0.0);
    return prefs->speed_priority;
}

/**
 * mcp_model_preferences_set_intelligence_priority:
 * @prefs: a #McpModelPreferences
 * @priority: the priority (0.0 to 1.0)
 *
 * Sets the intelligence priority.
 */
void
mcp_model_preferences_set_intelligence_priority (McpModelPreferences *prefs,
                                                 gdouble              priority)
{
    g_return_if_fail (prefs != NULL);
    prefs->intelligence_priority = CLAMP (priority, 0.0, 1.0);
}

/**
 * mcp_model_preferences_get_intelligence_priority:
 * @prefs: a #McpModelPreferences
 *
 * Gets the intelligence priority.
 *
 * Returns: the intelligence priority
 */
gdouble
mcp_model_preferences_get_intelligence_priority (McpModelPreferences *prefs)
{
    g_return_val_if_fail (prefs != NULL, 0.0);
    return prefs->intelligence_priority;
}

/**
 * mcp_model_preferences_to_json:
 * @prefs: a #McpModelPreferences
 *
 * Serializes the preferences to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *
mcp_model_preferences_to_json (McpModelPreferences *prefs)
{
    JsonBuilder *builder;
    JsonNode *node;
    GList *l;

    g_return_val_if_fail (prefs != NULL, NULL);

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    /* hints array */
    if (prefs->hints != NULL)
    {
        json_builder_set_member_name (builder, "hints");
        json_builder_begin_array (builder);
        for (l = prefs->hints; l != NULL; l = l->next)
        {
            json_builder_begin_object (builder);
            json_builder_set_member_name (builder, "name");
            json_builder_add_string_value (builder, (const gchar *) l->data);
            json_builder_end_object (builder);
        }
        json_builder_end_array (builder);
    }

    /* Priority values (only include if non-zero) */
    if (prefs->cost_priority > 0.0)
    {
        json_builder_set_member_name (builder, "costPriority");
        json_builder_add_double_value (builder, prefs->cost_priority);
    }

    if (prefs->speed_priority > 0.0)
    {
        json_builder_set_member_name (builder, "speedPriority");
        json_builder_add_double_value (builder, prefs->speed_priority);
    }

    if (prefs->intelligence_priority > 0.0)
    {
        json_builder_set_member_name (builder, "intelligencePriority");
        json_builder_add_double_value (builder, prefs->intelligence_priority);
    }

    json_builder_end_object (builder);
    node = json_builder_get_root (builder);
    g_object_unref (builder);

    return node;
}

/**
 * mcp_model_preferences_new_from_json:
 * @node: a #JsonNode containing preferences
 * @error: (nullable): return location for a #GError
 *
 * Creates new model preferences from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpModelPreferences, or %NULL on error
 */
McpModelPreferences *
mcp_model_preferences_new_from_json (JsonNode  *node,
                                     GError   **error)
{
    JsonObject *obj;
    McpModelPreferences *prefs;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Model preferences must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);
    prefs = mcp_model_preferences_new ();

    /* Parse hints array */
    if (json_object_has_member (obj, "hints"))
    {
        JsonArray *hints_arr = json_object_get_array_member (obj, "hints");
        guint i;
        guint len = json_array_get_length (hints_arr);

        for (i = 0; i < len; i++)
        {
            JsonObject *hint_obj = json_array_get_object_element (hints_arr, i);
            if (hint_obj != NULL && json_object_has_member (hint_obj, "name"))
            {
                mcp_model_preferences_add_hint (prefs,
                    json_object_get_string_member (hint_obj, "name"));
            }
        }
    }

    /* Parse priority values */
    if (json_object_has_member (obj, "costPriority"))
    {
        prefs->cost_priority = json_object_get_double_member (obj, "costPriority");
    }

    if (json_object_has_member (obj, "speedPriority"))
    {
        prefs->speed_priority = json_object_get_double_member (obj, "speedPriority");
    }

    if (json_object_has_member (obj, "intelligencePriority"))
    {
        prefs->intelligence_priority = json_object_get_double_member (obj, "intelligencePriority");
    }

    return prefs;
}

/* ========================================================================== */
/* McpSamplingResult                                                          */
/* ========================================================================== */

struct _McpSamplingResult
{
    gint     ref_count;
    McpRole  role;
    gchar   *text;
    gchar   *model;
    gchar   *stop_reason;
};

/**
 * mcp_sampling_result_new:
 * @role: the message role
 *
 * Creates a new sampling result.
 *
 * Returns: (transfer full): a new #McpSamplingResult
 */
McpSamplingResult *
mcp_sampling_result_new (McpRole role)
{
    McpSamplingResult *result;

    result = g_new0 (McpSamplingResult, 1);
    result->ref_count = 1;
    result->role = role;

    return result;
}

/**
 * mcp_sampling_result_ref:
 * @result: a #McpSamplingResult
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpSamplingResult
 */
McpSamplingResult *
mcp_sampling_result_ref (McpSamplingResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);

    result->ref_count++;
    return result;
}

/**
 * mcp_sampling_result_unref:
 * @result: a #McpSamplingResult
 *
 * Decrements the reference count.
 */
void
mcp_sampling_result_unref (McpSamplingResult *result)
{
    g_return_if_fail (result != NULL);

    if (--result->ref_count == 0)
    {
        g_free (result->text);
        g_free (result->model);
        g_free (result->stop_reason);
        g_free (result);
    }
}

/**
 * mcp_sampling_result_get_role:
 * @result: a #McpSamplingResult
 *
 * Gets the message role.
 *
 * Returns: the #McpRole
 */
McpRole
mcp_sampling_result_get_role (McpSamplingResult *result)
{
    g_return_val_if_fail (result != NULL, MCP_ROLE_ASSISTANT);
    return result->role;
}

/**
 * mcp_sampling_result_set_text:
 * @result: a #McpSamplingResult
 * @text: the text content
 *
 * Sets the text content of the result.
 */
void
mcp_sampling_result_set_text (McpSamplingResult *result,
                              const gchar       *text)
{
    g_return_if_fail (result != NULL);

    g_free (result->text);
    result->text = g_strdup (text);
}

/**
 * mcp_sampling_result_get_text:
 * @result: a #McpSamplingResult
 *
 * Gets the text content of the result.
 *
 * Returns: (transfer none) (nullable): the text content
 */
const gchar *
mcp_sampling_result_get_text (McpSamplingResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);
    return result->text;
}

/**
 * mcp_sampling_result_set_model:
 * @result: a #McpSamplingResult
 * @model: the model name
 *
 * Sets the model that generated this result.
 */
void
mcp_sampling_result_set_model (McpSamplingResult *result,
                               const gchar       *model)
{
    g_return_if_fail (result != NULL);

    g_free (result->model);
    result->model = g_strdup (model);
}

/**
 * mcp_sampling_result_get_model:
 * @result: a #McpSamplingResult
 *
 * Gets the model that generated this result.
 *
 * Returns: (transfer none) (nullable): the model name
 */
const gchar *
mcp_sampling_result_get_model (McpSamplingResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);
    return result->model;
}

/**
 * mcp_sampling_result_set_stop_reason:
 * @result: a #McpSamplingResult
 * @reason: the stop reason
 *
 * Sets the reason why generation stopped.
 */
void
mcp_sampling_result_set_stop_reason (McpSamplingResult *result,
                                     const gchar       *reason)
{
    g_return_if_fail (result != NULL);

    g_free (result->stop_reason);
    result->stop_reason = g_strdup (reason);
}

/**
 * mcp_sampling_result_get_stop_reason:
 * @result: a #McpSamplingResult
 *
 * Gets the reason why generation stopped.
 *
 * Returns: (transfer none) (nullable): the stop reason
 */
const gchar *
mcp_sampling_result_get_stop_reason (McpSamplingResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);
    return result->stop_reason;
}

/**
 * mcp_sampling_result_to_json:
 * @result: a #McpSamplingResult
 *
 * Serializes the result to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *
mcp_sampling_result_to_json (McpSamplingResult *result)
{
    JsonBuilder *builder;
    JsonNode *node;
    const gchar *role_str;

    g_return_val_if_fail (result != NULL, NULL);

    role_str = mcp_role_to_string (result->role);

    builder = json_builder_new ();
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "role");
    json_builder_add_string_value (builder, role_str);

    /* Content as object with type and text */
    json_builder_set_member_name (builder, "content");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "text");
    json_builder_set_member_name (builder, "text");
    json_builder_add_string_value (builder, result->text ? result->text : "");
    json_builder_end_object (builder);

    json_builder_set_member_name (builder, "model");
    json_builder_add_string_value (builder, result->model ? result->model : "");

    if (result->stop_reason != NULL)
    {
        json_builder_set_member_name (builder, "stopReason");
        json_builder_add_string_value (builder, result->stop_reason);
    }

    json_builder_end_object (builder);
    node = json_builder_get_root (builder);
    g_object_unref (builder);

    return node;
}

/**
 * mcp_sampling_result_new_from_json:
 * @node: a #JsonNode containing a result
 * @error: (nullable): return location for a #GError
 *
 * Creates a new sampling result from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpSamplingResult, or %NULL on error
 */
McpSamplingResult *
mcp_sampling_result_new_from_json (JsonNode  *node,
                                   GError   **error)
{
    JsonObject *obj;
    McpSamplingResult *result;
    const gchar *role_str;
    McpRole role;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Sampling result must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    /* Parse role */
    if (!json_object_has_member (obj, "role"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Sampling result missing required 'role' field");
        return NULL;
    }
    role_str = json_object_get_string_member (obj, "role");
    role = mcp_role_from_string (role_str);

    result = mcp_sampling_result_new (role);

    /* Parse content */
    if (json_object_has_member (obj, "content"))
    {
        JsonNode *content_node = json_object_get_member (obj, "content");
        if (JSON_NODE_HOLDS_OBJECT (content_node))
        {
            JsonObject *content_obj = json_node_get_object (content_node);
            if (json_object_has_member (content_obj, "text"))
            {
                mcp_sampling_result_set_text (result,
                    json_object_get_string_member (content_obj, "text"));
            }
        }
    }

    /* Parse model */
    if (json_object_has_member (obj, "model"))
    {
        mcp_sampling_result_set_model (result,
            json_object_get_string_member (obj, "model"));
    }

    /* Parse stop reason */
    if (json_object_has_member (obj, "stopReason"))
    {
        mcp_sampling_result_set_stop_reason (result,
            json_object_get_string_member (obj, "stopReason"));
    }

    return result;
}
