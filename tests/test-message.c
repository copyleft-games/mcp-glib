/*
 * test-message.c - Unit tests for MCP message types
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include "mcp.h"

/* ========================================================================== */
/* McpRequest Tests                                                           */
/* ========================================================================== */

/*
 * Test basic request creation
 */
static void
test_request_new (void)
{
    g_autoptr(McpRequest) request = NULL;

    request = mcp_request_new ("tools/list", "1");

    g_assert_nonnull (request);
    g_assert_cmpstr (mcp_request_get_method (request), ==, "tools/list");
    g_assert_cmpstr (mcp_request_get_id (request), ==, "1");
    g_assert_null (mcp_request_get_params (request));
    g_assert_cmpint (mcp_message_get_message_type (MCP_MESSAGE (request)), ==,
                     MCP_MESSAGE_TYPE_REQUEST);
}

/*
 * Test request creation with parameters
 */
static void
test_request_with_params (void)
{
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *params;
    JsonNode *result_params;

    /* Build params object */
    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "name");
    json_builder_add_string_value (builder, "test-tool");
    json_builder_set_member_name (builder, "arguments");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "x");
    json_builder_add_int_value (builder, 42);
    json_builder_end_object (builder);
    json_builder_end_object (builder);

    params = json_builder_get_root (builder);

    request = mcp_request_new_with_params ("tools/call", "42", params);

    g_assert_nonnull (request);
    g_assert_cmpstr (mcp_request_get_method (request), ==, "tools/call");
    g_assert_cmpstr (mcp_request_get_id (request), ==, "42");

    result_params = mcp_request_get_params (request);
    g_assert_nonnull (result_params);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (result_params));
}

/*
 * Test request JSON serialization
 */
static void
test_request_to_json (void)
{
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    request = mcp_request_new ("initialize", "init-1");

    json = mcp_message_to_json (MCP_MESSAGE (request));

    g_assert_nonnull (json);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (json));

    obj = json_node_get_object (json);

    g_assert_cmpstr (json_object_get_string_member (obj, "jsonrpc"), ==, "2.0");
    g_assert_cmpstr (json_object_get_string_member (obj, "id"), ==, "init-1");
    g_assert_cmpstr (json_object_get_string_member (obj, "method"), ==, "initialize");
    g_assert_false (json_object_has_member (obj, "params"));
}

/*
 * Test request JSON serialization with params
 */
static void
test_request_to_json_with_params (void)
{
    g_autoptr(McpRequest) request = NULL;
    g_autoptr(JsonNode) json = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *params;
    JsonObject *obj;

    /* Build params object */
    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "uri");
    json_builder_add_string_value (builder, "file:///test.txt");
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    request = mcp_request_new_with_params ("resources/read", "read-1", params);

    json = mcp_message_to_json (MCP_MESSAGE (request));

    g_assert_nonnull (json);
    obj = json_node_get_object (json);

    g_assert_true (json_object_has_member (obj, "params"));

    JsonObject *params_obj = json_object_get_object_member (obj, "params");
    g_assert_cmpstr (json_object_get_string_member (params_obj, "uri"), ==, "file:///test.txt");
}

/*
 * Test request to_string
 */
static void
test_request_to_string (void)
{
    g_autoptr(McpRequest) request = NULL;
    g_autofree gchar *str = NULL;

    request = mcp_request_new ("ping", "99");

    str = mcp_message_to_string (MCP_MESSAGE (request));

    g_assert_nonnull (str);
    g_assert_true (g_strstr_len (str, -1, "\"jsonrpc\":\"2.0\"") != NULL);
    g_assert_true (g_strstr_len (str, -1, "\"method\":\"ping\"") != NULL);
    g_assert_true (g_strstr_len (str, -1, "\"id\":\"99\"") != NULL);
}

/* ========================================================================== */
/* McpResponse Tests                                                          */
/* ========================================================================== */

/*
 * Test basic response creation
 */
static void
test_response_new (void)
{
    g_autoptr(McpResponse) response = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *result;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "success");
    json_builder_add_boolean_value (builder, TRUE);
    json_builder_end_object (builder);
    result = json_builder_get_root (builder);

    response = mcp_response_new ("1", result);

    g_assert_nonnull (response);
    g_assert_cmpstr (mcp_response_get_id (response), ==, "1");
    g_assert_nonnull (mcp_response_get_result (response));
    g_assert_cmpint (mcp_message_get_message_type (MCP_MESSAGE (response)), ==,
                     MCP_MESSAGE_TYPE_RESPONSE);
}

/*
 * Test response JSON serialization
 */
static void
test_response_to_json (void)
{
    g_autoptr(McpResponse) response = NULL;
    g_autoptr(JsonNode) json = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *result;
    JsonObject *obj;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "tools");
    json_builder_begin_array (builder);
    json_builder_end_array (builder);
    json_builder_end_object (builder);
    result = json_builder_get_root (builder);

    response = mcp_response_new ("list-1", result);

    json = mcp_message_to_json (MCP_MESSAGE (response));

    g_assert_nonnull (json);
    obj = json_node_get_object (json);

    g_assert_cmpstr (json_object_get_string_member (obj, "jsonrpc"), ==, "2.0");
    g_assert_cmpstr (json_object_get_string_member (obj, "id"), ==, "list-1");
    g_assert_true (json_object_has_member (obj, "result"));
    g_assert_false (json_object_has_member (obj, "error"));
}

/* ========================================================================== */
/* McpErrorResponse Tests                                                     */
/* ========================================================================== */

/*
 * Test basic error response creation
 */
static void
test_error_response_new (void)
{
    g_autoptr(McpErrorResponse) error = NULL;

    error = mcp_error_response_new ("1", MCP_ERROR_METHOD_NOT_FOUND, "Method not found");

    g_assert_nonnull (error);
    g_assert_cmpstr (mcp_error_response_get_id (error), ==, "1");
    g_assert_cmpint (mcp_error_response_get_code (error), ==, MCP_ERROR_METHOD_NOT_FOUND);
    g_assert_cmpstr (mcp_error_response_get_error_message (error), ==, "Method not found");
    g_assert_null (mcp_error_response_get_data (error));
    g_assert_cmpint (mcp_message_get_message_type (MCP_MESSAGE (error)), ==,
                     MCP_MESSAGE_TYPE_ERROR);
}

/*
 * Test error response with data
 */
static void
test_error_response_with_data (void)
{
    g_autoptr(McpErrorResponse) error = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *data;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "details");
    json_builder_add_string_value (builder, "Additional info");
    json_builder_end_object (builder);
    data = json_builder_get_root (builder);

    error = mcp_error_response_new_with_data ("2", MCP_ERROR_INVALID_PARAMS,
                                              "Invalid parameters", data);

    g_assert_nonnull (error);
    g_assert_cmpint (mcp_error_response_get_code (error), ==, MCP_ERROR_INVALID_PARAMS);
    g_assert_nonnull (mcp_error_response_get_data (error));
}

/*
 * Test error response with null ID (for parse errors)
 */
static void
test_error_response_null_id (void)
{
    g_autoptr(McpErrorResponse) error = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    error = mcp_error_response_new (NULL, MCP_ERROR_PARSE_ERROR, "Parse error");

    g_assert_nonnull (error);
    g_assert_null (mcp_error_response_get_id (error));

    json = mcp_message_to_json (MCP_MESSAGE (error));
    obj = json_node_get_object (json);

    g_assert_true (json_object_has_member (obj, "id"));
    g_assert_true (json_node_is_null (json_object_get_member (obj, "id")));
}

/*
 * Test error response from GError
 */
static void
test_error_response_from_gerror (void)
{
    g_autoptr(McpErrorResponse) error = NULL;
    g_autoptr(GError) gerror = NULL;

    gerror = g_error_new (MCP_ERROR, MCP_ERROR_INTERNAL_ERROR, "Something went wrong");

    error = mcp_error_response_new_from_gerror ("3", gerror);

    g_assert_nonnull (error);
    g_assert_cmpint (mcp_error_response_get_code (error), ==, MCP_ERROR_INTERNAL_ERROR);
    g_assert_cmpstr (mcp_error_response_get_error_message (error), ==, "Something went wrong");
}

/*
 * Test error response to GError conversion
 */
static void
test_error_response_to_gerror (void)
{
    g_autoptr(McpErrorResponse) error = NULL;
    g_autoptr(GError) gerror = NULL;

    error = mcp_error_response_new ("4", MCP_ERROR_INVALID_REQUEST, "Bad request");

    gerror = mcp_error_response_to_gerror (error);

    g_assert_nonnull (gerror);
    g_assert_cmpint (gerror->domain, ==, MCP_ERROR);
    g_assert_cmpint (gerror->code, ==, MCP_ERROR_INVALID_REQUEST);
    g_assert_cmpstr (gerror->message, ==, "Bad request");
}

/*
 * Test error response JSON serialization
 */
static void
test_error_response_to_json (void)
{
    g_autoptr(McpErrorResponse) error = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;
    JsonObject *error_obj;

    error = mcp_error_response_new ("5", MCP_ERROR_METHOD_NOT_FOUND, "Unknown method");

    json = mcp_message_to_json (MCP_MESSAGE (error));

    g_assert_nonnull (json);
    obj = json_node_get_object (json);

    g_assert_cmpstr (json_object_get_string_member (obj, "jsonrpc"), ==, "2.0");
    g_assert_cmpstr (json_object_get_string_member (obj, "id"), ==, "5");
    g_assert_true (json_object_has_member (obj, "error"));
    g_assert_false (json_object_has_member (obj, "result"));

    error_obj = json_object_get_object_member (obj, "error");
    g_assert_cmpint (json_object_get_int_member (error_obj, "code"), ==, MCP_ERROR_METHOD_NOT_FOUND);
    g_assert_cmpstr (json_object_get_string_member (error_obj, "message"), ==, "Unknown method");
}

/* ========================================================================== */
/* McpNotification Tests                                                      */
/* ========================================================================== */

/*
 * Test basic notification creation
 */
static void
test_notification_new (void)
{
    g_autoptr(McpNotification) notification = NULL;

    notification = mcp_notification_new ("notifications/cancelled");

    g_assert_nonnull (notification);
    g_assert_cmpstr (mcp_notification_get_method (notification), ==, "notifications/cancelled");
    g_assert_null (mcp_notification_get_params (notification));
    g_assert_cmpint (mcp_message_get_message_type (MCP_MESSAGE (notification)), ==,
                     MCP_MESSAGE_TYPE_NOTIFICATION);
}

/*
 * Test notification with params
 */
static void
test_notification_with_params (void)
{
    g_autoptr(McpNotification) notification = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *params;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "uri");
    json_builder_add_string_value (builder, "file:///changed.txt");
    json_builder_end_object (builder);
    params = json_builder_get_root (builder);

    notification = mcp_notification_new_with_params ("notifications/resources/updated", params);

    g_assert_nonnull (notification);
    g_assert_nonnull (mcp_notification_get_params (notification));
}

/*
 * Test notification JSON serialization (should NOT have id)
 */
static void
test_notification_to_json (void)
{
    g_autoptr(McpNotification) notification = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    notification = mcp_notification_new ("notifications/progress");

    json = mcp_message_to_json (MCP_MESSAGE (notification));

    g_assert_nonnull (json);
    obj = json_node_get_object (json);

    g_assert_cmpstr (json_object_get_string_member (obj, "jsonrpc"), ==, "2.0");
    g_assert_cmpstr (json_object_get_string_member (obj, "method"), ==, "notifications/progress");
    /* Notifications should NOT have an id field */
    g_assert_false (json_object_has_member (obj, "id"));
}

/* ========================================================================== */
/* Message Parsing Tests                                                      */
/* ========================================================================== */

/*
 * Test parsing a request from JSON string
 */
static void
test_message_parse_request (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    const gchar *json_str =
        "{"
        "  \"jsonrpc\": \"2.0\","
        "  \"id\": \"req-1\","
        "  \"method\": \"tools/list\","
        "  \"params\": { \"cursor\": null }"
        "}";

    message = mcp_message_parse (json_str, &error);

    g_assert_no_error (error);
    g_assert_nonnull (message);
    g_assert_true (MCP_IS_REQUEST (message));
    g_assert_cmpint (mcp_message_get_message_type (message), ==, MCP_MESSAGE_TYPE_REQUEST);

    McpRequest *request = MCP_REQUEST (message);
    g_assert_cmpstr (mcp_request_get_id (request), ==, "req-1");
    g_assert_cmpstr (mcp_request_get_method (request), ==, "tools/list");
    g_assert_nonnull (mcp_request_get_params (request));
}

/*
 * Test parsing a response from JSON string
 */
static void
test_message_parse_response (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    const gchar *json_str =
        "{"
        "  \"jsonrpc\": \"2.0\","
        "  \"id\": \"resp-1\","
        "  \"result\": { \"tools\": [] }"
        "}";

    message = mcp_message_parse (json_str, &error);

    g_assert_no_error (error);
    g_assert_nonnull (message);
    g_assert_true (MCP_IS_RESPONSE (message));
    g_assert_cmpint (mcp_message_get_message_type (message), ==, MCP_MESSAGE_TYPE_RESPONSE);

    McpResponse *response = MCP_RESPONSE (message);
    g_assert_cmpstr (mcp_response_get_id (response), ==, "resp-1");
}

/*
 * Test parsing an error response from JSON string
 */
static void
test_message_parse_error (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    const gchar *json_str =
        "{"
        "  \"jsonrpc\": \"2.0\","
        "  \"id\": \"err-1\","
        "  \"error\": {"
        "    \"code\": -32601,"
        "    \"message\": \"Method not found\""
        "  }"
        "}";

    message = mcp_message_parse (json_str, &error);

    g_assert_no_error (error);
    g_assert_nonnull (message);
    g_assert_true (MCP_IS_ERROR_RESPONSE (message));
    g_assert_cmpint (mcp_message_get_message_type (message), ==, MCP_MESSAGE_TYPE_ERROR);

    McpErrorResponse *err = MCP_ERROR_RESPONSE (message);
    g_assert_cmpstr (mcp_error_response_get_id (err), ==, "err-1");
    g_assert_cmpint (mcp_error_response_get_code (err), ==, -32601);
}

/*
 * Test parsing a notification from JSON string
 */
static void
test_message_parse_notification (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    const gchar *json_str =
        "{"
        "  \"jsonrpc\": \"2.0\","
        "  \"method\": \"notifications/initialized\""
        "}";

    message = mcp_message_parse (json_str, &error);

    g_assert_no_error (error);
    g_assert_nonnull (message);
    g_assert_true (MCP_IS_NOTIFICATION (message));
    g_assert_cmpint (mcp_message_get_message_type (message), ==, MCP_MESSAGE_TYPE_NOTIFICATION);

    McpNotification *notification = MCP_NOTIFICATION (message);
    g_assert_cmpstr (mcp_notification_get_method (notification), ==, "notifications/initialized");
}

/*
 * Test parsing invalid JSON
 */
static void
test_message_parse_invalid_json (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    const gchar *json_str = "{ not valid json }";

    message = mcp_message_parse (json_str, &error);

    g_assert_error (error, JSON_PARSER_ERROR, JSON_PARSER_ERROR_INVALID_BAREWORD);
    g_assert_null (message);
}

/*
 * Test parsing missing jsonrpc field
 */
static void
test_message_parse_missing_jsonrpc (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    const gchar *json_str =
        "{"
        "  \"id\": \"1\","
        "  \"method\": \"test\""
        "}";

    message = mcp_message_parse (json_str, &error);

    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_REQUEST);
    g_assert_null (message);
}

/*
 * Test parsing wrong jsonrpc version
 */
static void
test_message_parse_wrong_version (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    const gchar *json_str =
        "{"
        "  \"jsonrpc\": \"1.0\","
        "  \"id\": \"1\","
        "  \"method\": \"test\""
        "}";

    message = mcp_message_parse (json_str, &error);

    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_REQUEST);
    g_assert_null (message);
}

/* ========================================================================== */
/* Round-trip Tests                                                           */
/* ========================================================================== */

/*
 * Test request serialization round-trip
 */
static void
test_request_roundtrip (void)
{
    g_autoptr(McpRequest) original = NULL;
    g_autoptr(McpMessage) restored = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree gchar *json_str = NULL;

    original = mcp_request_new ("tools/call", "roundtrip-1");

    json_str = mcp_message_to_string (MCP_MESSAGE (original));
    restored = mcp_message_parse (json_str, &error);

    g_assert_no_error (error);
    g_assert_nonnull (restored);
    g_assert_true (MCP_IS_REQUEST (restored));

    McpRequest *restored_req = MCP_REQUEST (restored);
    g_assert_cmpstr (mcp_request_get_id (restored_req), ==, mcp_request_get_id (original));
    g_assert_cmpstr (mcp_request_get_method (restored_req), ==, mcp_request_get_method (original));
}

/*
 * Test response serialization round-trip
 */
static void
test_response_roundtrip (void)
{
    g_autoptr(McpResponse) original = NULL;
    g_autoptr(McpMessage) restored = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(JsonBuilder) builder = NULL;
    JsonNode *result;
    g_autofree gchar *json_str = NULL;

    builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "value");
    json_builder_add_int_value (builder, 42);
    json_builder_end_object (builder);
    result = json_builder_get_root (builder);

    original = mcp_response_new ("roundtrip-2", result);

    json_str = mcp_message_to_string (MCP_MESSAGE (original));
    restored = mcp_message_parse (json_str, &error);

    g_assert_no_error (error);
    g_assert_nonnull (restored);
    g_assert_true (MCP_IS_RESPONSE (restored));

    McpResponse *restored_resp = MCP_RESPONSE (restored);
    g_assert_cmpstr (mcp_response_get_id (restored_resp), ==, mcp_response_get_id (original));
}

/*
 * Test error response serialization round-trip
 */
static void
test_error_roundtrip (void)
{
    g_autoptr(McpErrorResponse) original = NULL;
    g_autoptr(McpMessage) restored = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree gchar *json_str = NULL;

    original = mcp_error_response_new ("roundtrip-3", MCP_ERROR_INVALID_PARAMS, "Bad params");

    json_str = mcp_message_to_string (MCP_MESSAGE (original));
    restored = mcp_message_parse (json_str, &error);

    g_assert_no_error (error);
    g_assert_nonnull (restored);
    g_assert_true (MCP_IS_ERROR_RESPONSE (restored));

    McpErrorResponse *restored_err = MCP_ERROR_RESPONSE (restored);
    g_assert_cmpstr (mcp_error_response_get_id (restored_err), ==, mcp_error_response_get_id (original));
    g_assert_cmpint (mcp_error_response_get_code (restored_err), ==, mcp_error_response_get_code (original));
    g_assert_cmpstr (mcp_error_response_get_error_message (restored_err), ==,
                     mcp_error_response_get_error_message (original));
}

/*
 * Test notification serialization round-trip
 */
static void
test_notification_roundtrip (void)
{
    g_autoptr(McpNotification) original = NULL;
    g_autoptr(McpMessage) restored = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree gchar *json_str = NULL;

    original = mcp_notification_new ("notifications/tools/list_changed");

    json_str = mcp_message_to_string (MCP_MESSAGE (original));
    restored = mcp_message_parse (json_str, &error);

    g_assert_no_error (error);
    g_assert_nonnull (restored);
    g_assert_true (MCP_IS_NOTIFICATION (restored));

    McpNotification *restored_notif = MCP_NOTIFICATION (restored);
    g_assert_cmpstr (mcp_notification_get_method (restored_notif), ==,
                     mcp_notification_get_method (original));
}


/*
 * JSON-RPC 2.0 section 5: the response id MUST be identical to the
 * request's.  A strict client compares the TYPE too, so a numeric id
 * answered as a string fails the handshake -- with an error that reads
 * absurdly, along the lines of "expected 0, got 0", because both sides
 * print the same.
 *
 * This shipped for a long time because ids are modelled here as strings
 * and lenient clients never complained.  The cost was an agent session
 * with no tools at all and nothing in any log to say why.
 */
static void
test_numeric_id_stays_numeric (void)
{
    const gchar *wire =
        "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"initialize\"}";
    g_autoptr (McpMessage) msg = NULL;
    g_autoptr (McpResponse) response = NULL;
    g_autoptr (JsonNode) node = NULL;
    g_autofree gchar *out = NULL;
    JsonNode *result;
    McpRequest *request;

    msg = mcp_message_parse (wire, NULL);
    g_assert_nonnull (msg);
    g_assert_true (MCP_IS_REQUEST (msg));

    request = MCP_REQUEST (msg);
    g_assert_cmpstr (mcp_request_get_id (request), ==, "0");
    g_assert_true (mcp_request_get_id_is_number (request));

    result = json_node_new (JSON_NODE_OBJECT);
    json_node_take_object (result, json_object_new ());
    response = mcp_response_new (mcp_request_get_id (request), result);
    mcp_response_set_id_is_number (response,
                                   mcp_request_get_id_is_number (request));

    node = mcp_message_to_json (MCP_MESSAGE (response));
    out = json_to_string (node, FALSE);

    /* Unquoted, as it arrived. */
    g_assert_nonnull (strstr (out, "\"id\":0"));
    g_assert_null (strstr (out, "\"id\":\"0\""));
}

static void
test_string_id_stays_a_string (void)
{
    /* The other half of the same rule: an id that arrived quoted must
     * not come back as a number just because it looks like one. */
    const gchar *wire =
        "{\"jsonrpc\":\"2.0\",\"id\":\"7\",\"method\":\"initialize\"}";
    g_autoptr (McpMessage) msg = NULL;
    g_autoptr (McpResponse) response = NULL;
    g_autoptr (JsonNode) node = NULL;
    g_autofree gchar *out = NULL;
    JsonNode *result;
    McpRequest *request;

    msg = mcp_message_parse (wire, NULL);
    g_assert_nonnull (msg);

    request = MCP_REQUEST (msg);
    g_assert_cmpstr (mcp_request_get_id (request), ==, "7");
    g_assert_false (mcp_request_get_id_is_number (request));

    result = json_node_new (JSON_NODE_OBJECT);
    json_node_take_object (result, json_object_new ());
    response = mcp_response_new (mcp_request_get_id (request), result);
    mcp_response_set_id_is_number (response,
                                   mcp_request_get_id_is_number (request));

    node = mcp_message_to_json (MCP_MESSAGE (response));
    out = json_to_string (node, FALSE);

    g_assert_nonnull (strstr (out, "\"id\":\"7\""));
}

static void
test_error_response_matches_the_id_type (void)
{
    /* An error reply has to match the request's id type just as a
     * success reply does; a client that rejects the type rejects both. */
    g_autoptr (McpErrorResponse) err = NULL;
    g_autoptr (JsonNode) node = NULL;
    g_autofree gchar *out = NULL;

    err = mcp_error_response_new ("42", -32601, "method not found");
    mcp_error_response_set_id_is_number (err, TRUE);

    node = mcp_message_to_json (MCP_MESSAGE (err));
    out = json_to_string (node, FALSE);

    g_assert_nonnull (strstr (out, "\"id\":42"));
    g_assert_null (strstr (out, "\"id\":\"42\""));
}


/*
 * A NULL return from mcp_message_new_from_json() must always carry a
 * GError: both McpServer::on_message_received() and
 * McpClient::on_message_received() print error->message on that path, so
 * a NULL return with no error set is a segfault in whichever direction
 * the frame arrived from.  Every case below used to reach
 * mcp_request_new()/mcp_response_new()/mcp_notification_new() with a NULL
 * argument, trip a precondition, and return NULL silently.
 */
static void
test_message_parse_null_id_request (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    message = mcp_message_parse (
        "{\"jsonrpc\": \"2.0\", \"id\": null, \"method\": \"ping\"}", &error);

    g_assert_null (message);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_REQUEST);
}

static void
test_message_parse_object_id_request (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    message = mcp_message_parse (
        "{\"jsonrpc\": \"2.0\", \"id\": {\"a\": 1}, \"method\": \"ping\"}", &error);

    g_assert_null (message);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_REQUEST);
}

static void
test_message_parse_non_string_method (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    message = mcp_message_parse (
        "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": 7}", &error);

    g_assert_null (message);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_REQUEST);
}

static void
test_message_parse_non_string_notification_method (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    message = mcp_message_parse (
        "{\"jsonrpc\": \"2.0\", \"method\": [\"a\"]}", &error);

    g_assert_null (message);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_REQUEST);
}

static void
test_message_parse_null_id_response (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    message = mcp_message_parse (
        "{\"jsonrpc\": \"2.0\", \"id\": null, \"result\": {}}", &error);

    g_assert_null (message);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_REQUEST);
}

static void
test_message_parse_non_string_error_message (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    message = mcp_message_parse (
        "{\"jsonrpc\": \"2.0\", \"id\": 1,"
        " \"error\": {\"code\": -1, \"message\": {\"nope\": 1}}}", &error);

    g_assert_null (message);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_REQUEST);
}

/*
 * An error response is the one place JSON-RPC allows a null id: a parse
 * error has no request to name.  That must keep working.
 */
static void
test_message_parse_null_id_error_response (void)
{
    g_autoptr(McpMessage) message = NULL;
    g_autoptr(GError) error = NULL;

    message = mcp_message_parse (
        "{\"jsonrpc\": \"2.0\", \"id\": null,"
        " \"error\": {\"code\": -32700, \"message\": \"Parse error\"}}", &error);

    g_assert_no_error (error);
    g_assert_nonnull (message);
    g_assert_cmpint (mcp_message_get_message_type (message), ==, MCP_MESSAGE_TYPE_ERROR);
    g_assert_null (mcp_error_response_get_id (MCP_ERROR_RESPONSE (message)));
}


int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* McpRequest tests */
    g_test_add_func ("/mcp/message/request/new", test_request_new);
    g_test_add_func ("/mcp/message/request/with-params", test_request_with_params);
    g_test_add_func ("/mcp/message/request/to-json", test_request_to_json);
    g_test_add_func ("/mcp/message/request/to-json-with-params", test_request_to_json_with_params);
    g_test_add_func ("/mcp/message/request/to-string", test_request_to_string);

    /* McpResponse tests */
    g_test_add_func ("/mcp/message/response/new", test_response_new);
    g_test_add_func ("/mcp/message/response/to-json", test_response_to_json);

    /* McpErrorResponse tests */
    g_test_add_func ("/mcp/message/error/new", test_error_response_new);
    g_test_add_func ("/mcp/message/error/with-data", test_error_response_with_data);
    g_test_add_func ("/mcp/message/error/null-id", test_error_response_null_id);
    g_test_add_func ("/mcp/message/error/from-gerror", test_error_response_from_gerror);
    g_test_add_func ("/mcp/message/error/to-gerror", test_error_response_to_gerror);
    g_test_add_func ("/mcp/message/error/to-json", test_error_response_to_json);

    /* McpNotification tests */
    g_test_add_func ("/mcp/message/notification/new", test_notification_new);
    g_test_add_func ("/mcp/message/notification/with-params", test_notification_with_params);
    g_test_add_func ("/mcp/message/notification/to-json", test_notification_to_json);

    /* Message parsing tests */
    g_test_add_func ("/mcp/message/parse/request", test_message_parse_request);
    g_test_add_func ("/mcp/message/parse/response", test_message_parse_response);
    g_test_add_func ("/mcp/message/parse/error", test_message_parse_error);
    g_test_add_func ("/mcp/message/parse/notification", test_message_parse_notification);
    g_test_add_func ("/mcp/message/parse/invalid-json", test_message_parse_invalid_json);
    g_test_add_func ("/mcp/message/parse/missing-jsonrpc", test_message_parse_missing_jsonrpc);
    g_test_add_func ("/mcp/message/parse/wrong-version", test_message_parse_wrong_version);

    /* A NULL return must always carry a GError */
    g_test_add_func ("/mcp/message/parse/null-id-request",
                     test_message_parse_null_id_request);
    g_test_add_func ("/mcp/message/parse/object-id-request",
                     test_message_parse_object_id_request);
    g_test_add_func ("/mcp/message/parse/non-string-method",
                     test_message_parse_non_string_method);
    g_test_add_func ("/mcp/message/parse/non-string-notification-method",
                     test_message_parse_non_string_notification_method);
    g_test_add_func ("/mcp/message/parse/null-id-response",
                     test_message_parse_null_id_response);
    g_test_add_func ("/mcp/message/parse/non-string-error-message",
                     test_message_parse_non_string_error_message);
    g_test_add_func ("/mcp/message/parse/null-id-error-response",
                     test_message_parse_null_id_error_response);

    /* Round-trip tests */
    g_test_add_func ("/mcp/message/id/numeric-stays-numeric",
                     test_numeric_id_stays_numeric);
    g_test_add_func ("/mcp/message/id/string-stays-string",
                     test_string_id_stays_a_string);
    g_test_add_func ("/mcp/message/id/error-matches-type",
                     test_error_response_matches_the_id_type);
    g_test_add_func ("/mcp/message/roundtrip/request", test_request_roundtrip);
    g_test_add_func ("/mcp/message/roundtrip/response", test_response_roundtrip);
    g_test_add_func ("/mcp/message/roundtrip/error", test_error_roundtrip);
    g_test_add_func ("/mcp/message/roundtrip/notification", test_notification_roundtrip);

    return g_test_run ();
}
