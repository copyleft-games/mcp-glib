/*
 * mcp-sampling.h - Sampling types for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This file defines the types used for sampling (server-to-client LLM requests):
 * - McpSamplingMessage: A message in a sampling request
 * - McpSamplingResult: The result from a sampling request
 * - McpModelPreferences: Preferences for model selection
 */

#ifndef MCP_SAMPLING_H
#define MCP_SAMPLING_H


#include <glib-object.h>
#include <json-glib/json-glib.h>
#include "mcp-enums.h"

G_BEGIN_DECLS

/* ========================================================================== */
/* McpSamplingMessage                                                         */
/* ========================================================================== */

/**
 * McpSamplingMessage:
 *
 * A message in a sampling request, containing role and content items.
 * Similar to prompt messages but used for sampling/createMessage requests.
 */
typedef struct _McpSamplingMessage McpSamplingMessage;

/**
 * mcp_sampling_message_new:
 * @role: the message role (user or assistant)
 *
 * Creates a new sampling message.
 *
 * Returns: (transfer full): a new #McpSamplingMessage
 */
McpSamplingMessage *mcp_sampling_message_new (McpRole role);

/**
 * mcp_sampling_message_ref:
 * @message: a #McpSamplingMessage
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpSamplingMessage
 */
McpSamplingMessage *mcp_sampling_message_ref (McpSamplingMessage *message);

/**
 * mcp_sampling_message_unref:
 * @message: a #McpSamplingMessage
 *
 * Decrements the reference count.
 */
void mcp_sampling_message_unref (McpSamplingMessage *message);

/**
 * mcp_sampling_message_get_role:
 * @message: a #McpSamplingMessage
 *
 * Gets the message role.
 *
 * Returns: the #McpRole
 */
McpRole mcp_sampling_message_get_role (McpSamplingMessage *message);

/**
 * mcp_sampling_message_add_text:
 * @message: a #McpSamplingMessage
 * @text: the text content
 *
 * Adds a text content item to the message.
 */
void mcp_sampling_message_add_text (McpSamplingMessage *message,
                                    const gchar        *text);

/**
 * mcp_sampling_message_add_image:
 * @message: a #McpSamplingMessage
 * @data: the base64-encoded image data
 * @mime_type: the MIME type
 *
 * Adds an image content item to the message.
 */
void mcp_sampling_message_add_image (McpSamplingMessage *message,
                                     const gchar        *data,
                                     const gchar        *mime_type);

/**
 * mcp_sampling_message_get_content:
 * @message: a #McpSamplingMessage
 *
 * Gets the content items as a JSON array.
 *
 * Returns: (transfer none): the content array
 */
JsonArray *mcp_sampling_message_get_content (McpSamplingMessage *message);

/**
 * mcp_sampling_message_to_json:
 * @message: a #McpSamplingMessage
 *
 * Serializes the message to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_sampling_message_to_json (McpSamplingMessage *message);

/**
 * mcp_sampling_message_new_from_json:
 * @node: a #JsonNode containing a message
 * @error: (nullable): return location for a #GError
 *
 * Creates a new sampling message from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpSamplingMessage, or %NULL on error
 */
McpSamplingMessage *mcp_sampling_message_new_from_json (JsonNode  *node,
                                                        GError   **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (McpSamplingMessage, mcp_sampling_message_unref)

/* ========================================================================== */
/* McpModelPreferences                                                        */
/* ========================================================================== */

/**
 * McpModelPreferences:
 *
 * Preferences for model selection in sampling requests.
 * Contains model hints and priority weights for cost, speed, and intelligence.
 */
typedef struct _McpModelPreferences McpModelPreferences;

/**
 * mcp_model_preferences_new:
 *
 * Creates a new model preferences object with default values.
 *
 * Returns: (transfer full): a new #McpModelPreferences
 */
McpModelPreferences *mcp_model_preferences_new (void);

/**
 * mcp_model_preferences_ref:
 * @prefs: a #McpModelPreferences
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpModelPreferences
 */
McpModelPreferences *mcp_model_preferences_ref (McpModelPreferences *prefs);

/**
 * mcp_model_preferences_unref:
 * @prefs: a #McpModelPreferences
 *
 * Decrements the reference count.
 */
void mcp_model_preferences_unref (McpModelPreferences *prefs);

/**
 * mcp_model_preferences_add_hint:
 * @prefs: a #McpModelPreferences
 * @name: the model name hint
 *
 * Adds a model name hint.
 */
void mcp_model_preferences_add_hint (McpModelPreferences *prefs,
                                     const gchar         *name);

/**
 * mcp_model_preferences_get_hints:
 * @prefs: a #McpModelPreferences
 *
 * Gets the list of model hints.
 *
 * Returns: (transfer none) (element-type utf8): the hints list
 */
GList *mcp_model_preferences_get_hints (McpModelPreferences *prefs);

/**
 * mcp_model_preferences_set_cost_priority:
 * @prefs: a #McpModelPreferences
 * @priority: the priority (0.0 to 1.0)
 *
 * Sets the cost priority. Higher values prioritize lower cost.
 */
void mcp_model_preferences_set_cost_priority (McpModelPreferences *prefs,
                                              gdouble              priority);

/**
 * mcp_model_preferences_get_cost_priority:
 * @prefs: a #McpModelPreferences
 *
 * Gets the cost priority.
 *
 * Returns: the cost priority
 */
gdouble mcp_model_preferences_get_cost_priority (McpModelPreferences *prefs);

/**
 * mcp_model_preferences_set_speed_priority:
 * @prefs: a #McpModelPreferences
 * @priority: the priority (0.0 to 1.0)
 *
 * Sets the speed priority. Higher values prioritize faster responses.
 */
void mcp_model_preferences_set_speed_priority (McpModelPreferences *prefs,
                                               gdouble              priority);

/**
 * mcp_model_preferences_get_speed_priority:
 * @prefs: a #McpModelPreferences
 *
 * Gets the speed priority.
 *
 * Returns: the speed priority
 */
gdouble mcp_model_preferences_get_speed_priority (McpModelPreferences *prefs);

/**
 * mcp_model_preferences_set_intelligence_priority:
 * @prefs: a #McpModelPreferences
 * @priority: the priority (0.0 to 1.0)
 *
 * Sets the intelligence priority. Higher values prioritize more capable models.
 */
void mcp_model_preferences_set_intelligence_priority (McpModelPreferences *prefs,
                                                      gdouble              priority);

/**
 * mcp_model_preferences_get_intelligence_priority:
 * @prefs: a #McpModelPreferences
 *
 * Gets the intelligence priority.
 *
 * Returns: the intelligence priority
 */
gdouble mcp_model_preferences_get_intelligence_priority (McpModelPreferences *prefs);

/**
 * mcp_model_preferences_to_json:
 * @prefs: a #McpModelPreferences
 *
 * Serializes the preferences to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_model_preferences_to_json (McpModelPreferences *prefs);

/**
 * mcp_model_preferences_new_from_json:
 * @node: a #JsonNode containing preferences
 * @error: (nullable): return location for a #GError
 *
 * Creates new model preferences from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpModelPreferences, or %NULL on error
 */
McpModelPreferences *mcp_model_preferences_new_from_json (JsonNode  *node,
                                                          GError   **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (McpModelPreferences, mcp_model_preferences_unref)

/* ========================================================================== */
/* McpSamplingResult                                                          */
/* ========================================================================== */

/**
 * McpSamplingResult:
 *
 * The result from a sampling request.
 * Contains the generated message content, model used, and stop reason.
 */
typedef struct _McpSamplingResult McpSamplingResult;

/**
 * mcp_sampling_result_new:
 * @role: the message role
 *
 * Creates a new sampling result.
 *
 * Returns: (transfer full): a new #McpSamplingResult
 */
McpSamplingResult *mcp_sampling_result_new (McpRole role);

/**
 * mcp_sampling_result_ref:
 * @result: a #McpSamplingResult
 *
 * Increments the reference count.
 *
 * Returns: (transfer full): the #McpSamplingResult
 */
McpSamplingResult *mcp_sampling_result_ref (McpSamplingResult *result);

/**
 * mcp_sampling_result_unref:
 * @result: a #McpSamplingResult
 *
 * Decrements the reference count.
 */
void mcp_sampling_result_unref (McpSamplingResult *result);

/**
 * mcp_sampling_result_get_role:
 * @result: a #McpSamplingResult
 *
 * Gets the message role.
 *
 * Returns: the #McpRole
 */
McpRole mcp_sampling_result_get_role (McpSamplingResult *result);

/**
 * mcp_sampling_result_set_text:
 * @result: a #McpSamplingResult
 * @text: the text content
 *
 * Sets the text content of the result.
 */
void mcp_sampling_result_set_text (McpSamplingResult *result,
                                   const gchar       *text);

/**
 * mcp_sampling_result_get_text:
 * @result: a #McpSamplingResult
 *
 * Gets the text content of the result.
 *
 * Returns: (transfer none) (nullable): the text content
 */
const gchar *mcp_sampling_result_get_text (McpSamplingResult *result);

/**
 * mcp_sampling_result_set_model:
 * @result: a #McpSamplingResult
 * @model: the model name
 *
 * Sets the model that generated this result.
 */
void mcp_sampling_result_set_model (McpSamplingResult *result,
                                    const gchar       *model);

/**
 * mcp_sampling_result_get_model:
 * @result: a #McpSamplingResult
 *
 * Gets the model that generated this result.
 *
 * Returns: (transfer none) (nullable): the model name
 */
const gchar *mcp_sampling_result_get_model (McpSamplingResult *result);

/**
 * mcp_sampling_result_set_stop_reason:
 * @result: a #McpSamplingResult
 * @reason: the stop reason
 *
 * Sets the reason why generation stopped.
 */
void mcp_sampling_result_set_stop_reason (McpSamplingResult *result,
                                          const gchar       *reason);

/**
 * mcp_sampling_result_get_stop_reason:
 * @result: a #McpSamplingResult
 *
 * Gets the reason why generation stopped.
 *
 * Returns: (transfer none) (nullable): the stop reason
 */
const gchar *mcp_sampling_result_get_stop_reason (McpSamplingResult *result);

/**
 * mcp_sampling_result_to_json:
 * @result: a #McpSamplingResult
 *
 * Serializes the result to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *mcp_sampling_result_to_json (McpSamplingResult *result);

/**
 * mcp_sampling_result_new_from_json:
 * @node: a #JsonNode containing a result
 * @error: (nullable): return location for a #GError
 *
 * Creates a new sampling result from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpSamplingResult, or %NULL on error
 */
McpSamplingResult *mcp_sampling_result_new_from_json (JsonNode  *node,
                                                      GError   **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (McpSamplingResult, mcp_sampling_result_unref)

G_END_DECLS

#endif /* MCP_SAMPLING_H */
