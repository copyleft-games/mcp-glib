/*
 * test-server.c - Tests for McpServer and provider types
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define MCP_COMPILATION
#include <mcp/mcp.h>
#undef MCP_COMPILATION

/* ========================================================================== */
/* McpToolResult tests                                                        */
/* ========================================================================== */

static void
test_tool_result_new (void)
{
    McpToolResult *result;

    result = mcp_tool_result_new (FALSE);
    g_assert_nonnull (result);
    g_assert_false (mcp_tool_result_get_is_error (result));

    mcp_tool_result_unref (result);
}

static void
test_tool_result_error (void)
{
    McpToolResult *result;

    result = mcp_tool_result_new (TRUE);
    g_assert_nonnull (result);
    g_assert_true (mcp_tool_result_get_is_error (result));

    mcp_tool_result_unref (result);
}

static void
test_tool_result_add_text (void)
{
    g_autoptr(McpToolResult) result = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;
    JsonArray *content;

    result = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_text (result, "Hello, world!");

    node = mcp_tool_result_to_json (result);
    g_assert_nonnull (node);

    obj = json_node_get_object (node);
    g_assert_nonnull (obj);

    content = json_object_get_array_member (obj, "content");
    g_assert_nonnull (content);
    g_assert_cmpuint (json_array_get_length (content), ==, 1);

    {
        JsonObject *item = json_array_get_object_element (content, 0);
        g_assert_cmpstr (json_object_get_string_member (item, "type"), ==, "text");
        g_assert_cmpstr (json_object_get_string_member (item, "text"), ==, "Hello, world!");
    }
}

static void
test_tool_result_add_image (void)
{
    g_autoptr(McpToolResult) result = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;
    JsonArray *content;

    result = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_image (result, "base64data==", "image/png");

    node = mcp_tool_result_to_json (result);
    obj = json_node_get_object (node);
    content = json_object_get_array_member (obj, "content");
    g_assert_cmpuint (json_array_get_length (content), ==, 1);

    {
        JsonObject *item = json_array_get_object_element (content, 0);
        g_assert_cmpstr (json_object_get_string_member (item, "type"), ==, "image");
        g_assert_cmpstr (json_object_get_string_member (item, "data"), ==, "base64data==");
        g_assert_cmpstr (json_object_get_string_member (item, "mimeType"), ==, "image/png");
    }
}

/* ========================================================================== */
/* McpResourceContents tests                                                  */
/* ========================================================================== */

static void
test_resource_contents_text (void)
{
    g_autoptr(McpResourceContents) contents = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;

    contents = mcp_resource_contents_new_text ("file:///test.txt", "Hello!", "text/plain");
    g_assert_nonnull (contents);
    g_assert_cmpstr (mcp_resource_contents_get_uri (contents), ==, "file:///test.txt");
    g_assert_cmpstr (mcp_resource_contents_get_mime_type (contents), ==, "text/plain");
    g_assert_cmpstr (mcp_resource_contents_get_text (contents), ==, "Hello!");
    g_assert_null (mcp_resource_contents_get_blob (contents));

    node = mcp_resource_contents_to_json (contents);
    obj = json_node_get_object (node);
    g_assert_cmpstr (json_object_get_string_member (obj, "uri"), ==, "file:///test.txt");
    g_assert_cmpstr (json_object_get_string_member (obj, "mimeType"), ==, "text/plain");
    g_assert_cmpstr (json_object_get_string_member (obj, "text"), ==, "Hello!");
    g_assert_false (json_object_has_member (obj, "blob"));
}

static void
test_resource_contents_blob (void)
{
    g_autoptr(McpResourceContents) contents = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;

    contents = mcp_resource_contents_new_blob ("file:///image.png", "YmluYXJ5ZGF0YQ==", "image/png");
    g_assert_nonnull (contents);
    g_assert_cmpstr (mcp_resource_contents_get_uri (contents), ==, "file:///image.png");
    g_assert_cmpstr (mcp_resource_contents_get_blob (contents), ==, "YmluYXJ5ZGF0YQ==");
    g_assert_null (mcp_resource_contents_get_text (contents));

    node = mcp_resource_contents_to_json (contents);
    obj = json_node_get_object (node);
    g_assert_cmpstr (json_object_get_string_member (obj, "blob"), ==, "YmluYXJ5ZGF0YQ==");
}

/* ========================================================================== */
/* McpPromptMessage tests                                                     */
/* ========================================================================== */

static void
test_prompt_message_new (void)
{
    g_autoptr(McpPromptMessage) msg = NULL;

    msg = mcp_prompt_message_new (MCP_ROLE_USER);
    g_assert_nonnull (msg);
    g_assert_cmpint (mcp_prompt_message_get_role (msg), ==, MCP_ROLE_USER);
}

static void
test_prompt_message_add_text (void)
{
    g_autoptr(McpPromptMessage) msg = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;
    JsonArray *content;

    msg = mcp_prompt_message_new (MCP_ROLE_ASSISTANT);
    mcp_prompt_message_add_text (msg, "Hello from assistant!");

    node = mcp_prompt_message_to_json (msg);
    obj = json_node_get_object (node);

    g_assert_cmpstr (json_object_get_string_member (obj, "role"), ==, "assistant");

    content = mcp_prompt_message_get_content (msg);
    g_assert_nonnull (content);
    g_assert_cmpuint (json_array_get_length (content), ==, 1);

    {
        JsonObject *item = json_array_get_object_element (content, 0);
        g_assert_cmpstr (json_object_get_string_member (item, "type"), ==, "text");
        g_assert_cmpstr (json_object_get_string_member (item, "text"), ==, "Hello from assistant!");
    }
}

static void
test_prompt_message_add_image (void)
{
    g_autoptr(McpPromptMessage) msg = NULL;
    JsonArray *content;

    msg = mcp_prompt_message_new (MCP_ROLE_USER);
    mcp_prompt_message_add_image (msg, "imagedata==", "image/jpeg");

    content = mcp_prompt_message_get_content (msg);
    g_assert_cmpuint (json_array_get_length (content), ==, 1);

    {
        JsonObject *item = json_array_get_object_element (content, 0);
        g_assert_cmpstr (json_object_get_string_member (item, "type"), ==, "image");
        g_assert_cmpstr (json_object_get_string_member (item, "data"), ==, "imagedata==");
        g_assert_cmpstr (json_object_get_string_member (item, "mimeType"), ==, "image/jpeg");
    }
}

/* ========================================================================== */
/* McpPromptResult tests                                                      */
/* ========================================================================== */

static void
test_prompt_result_new (void)
{
    g_autoptr(McpPromptResult) result = NULL;

    result = mcp_prompt_result_new ("Test description");
    g_assert_nonnull (result);
    g_assert_cmpstr (mcp_prompt_result_get_description (result), ==, "Test description");
}

static void
test_prompt_result_messages (void)
{
    g_autoptr(McpPromptResult) result = NULL;
    g_autoptr(McpPromptMessage) msg = NULL;
    GList *messages;

    result = mcp_prompt_result_new (NULL);
    msg = mcp_prompt_message_new (MCP_ROLE_USER);
    mcp_prompt_message_add_text (msg, "Test message");

    mcp_prompt_result_add_message (result, msg);

    messages = mcp_prompt_result_get_messages (result);
    g_assert_nonnull (messages);
    g_assert_cmpuint (g_list_length (messages), ==, 1);
}

static void
test_prompt_result_to_json (void)
{
    g_autoptr(McpPromptResult) result = NULL;
    g_autoptr(McpPromptMessage) msg = NULL;
    g_autoptr(JsonNode) node = NULL;
    JsonObject *obj;
    JsonArray *messages;

    result = mcp_prompt_result_new ("A prompt description");
    msg = mcp_prompt_message_new (MCP_ROLE_USER);
    mcp_prompt_message_add_text (msg, "Hello");
    mcp_prompt_result_add_message (result, msg);

    node = mcp_prompt_result_to_json (result);
    obj = json_node_get_object (node);

    g_assert_cmpstr (json_object_get_string_member (obj, "description"), ==, "A prompt description");

    messages = json_object_get_array_member (obj, "messages");
    g_assert_cmpuint (json_array_get_length (messages), ==, 1);
}

/* ========================================================================== */
/* McpServer tests                                                            */
/* ========================================================================== */

static void
test_server_new (void)
{
    g_autoptr(McpServer) server = NULL;
    McpImplementation *impl;

    server = mcp_server_new ("test-server", "1.0.0");
    g_assert_nonnull (server);

    impl = mcp_session_get_local_implementation (MCP_SESSION (server));
    g_assert_nonnull (impl);
    g_assert_cmpstr (mcp_implementation_get_name (impl), ==, "test-server");
    g_assert_cmpstr (mcp_implementation_get_version (impl), ==, "1.0.0");
}

static void
test_server_capabilities (void)
{
    g_autoptr(McpServer) server = NULL;
    McpServerCapabilities *caps;

    server = mcp_server_new ("test-server", "1.0.0");
    caps = mcp_server_get_capabilities (server);

    g_assert_nonnull (caps);
    /* Initially no capabilities are enabled */
    g_assert_false (mcp_server_capabilities_get_tools (caps));
    g_assert_false (mcp_server_capabilities_get_resources (caps));
    g_assert_false (mcp_server_capabilities_get_prompts (caps));
}

static void
test_server_instructions (void)
{
    g_autoptr(McpServer) server = NULL;

    server = mcp_server_new ("test-server", "1.0.0");
    g_assert_null (mcp_server_get_instructions (server));

    mcp_server_set_instructions (server, "Use this server for testing.");
    g_assert_cmpstr (mcp_server_get_instructions (server), ==, "Use this server for testing.");
}

static void
test_server_add_tool (void)
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpTool) tool = NULL;
    GList *tools;
    McpServerCapabilities *caps;

    server = mcp_server_new ("test-server", "1.0.0");
    tool = mcp_tool_new ("echo", "Echoes the input");

    mcp_server_add_tool (server, tool, NULL, NULL, NULL);

    tools = mcp_server_list_tools (server);
    g_assert_cmpuint (g_list_length (tools), ==, 1);
    g_list_free_full (tools, g_object_unref);

    /* Check capability was enabled */
    caps = mcp_server_get_capabilities (server);
    g_assert_true (mcp_server_capabilities_get_tools (caps));
}

static void
test_server_remove_tool (void)
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpTool) tool = NULL;
    GList *tools;
    gboolean removed;

    server = mcp_server_new ("test-server", "1.0.0");
    tool = mcp_tool_new ("echo", "Echoes the input");

    mcp_server_add_tool (server, tool, NULL, NULL, NULL);
    removed = mcp_server_remove_tool (server, "echo");
    g_assert_true (removed);

    tools = mcp_server_list_tools (server);
    g_assert_cmpuint (g_list_length (tools), ==, 0);

    /* Removing non-existent tool returns FALSE */
    removed = mcp_server_remove_tool (server, "nonexistent");
    g_assert_false (removed);
}

static void
test_server_add_resource (void)
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpResource) resource = NULL;
    GList *resources;
    McpServerCapabilities *caps;

    server = mcp_server_new ("test-server", "1.0.0");
    resource = mcp_resource_new ("file:///test.txt", "test.txt");

    mcp_server_add_resource (server, resource, NULL, NULL, NULL);

    resources = mcp_server_list_resources (server);
    g_assert_cmpuint (g_list_length (resources), ==, 1);
    g_list_free_full (resources, g_object_unref);

    /* Check capability was enabled */
    caps = mcp_server_get_capabilities (server);
    g_assert_true (mcp_server_capabilities_get_resources (caps));
}

static void
test_server_add_resource_template (void)
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpResourceTemplate) templ = NULL;
    GList *templates;

    server = mcp_server_new ("test-server", "1.0.0");
    templ = mcp_resource_template_new ("file:///{path}", "File");

    mcp_server_add_resource_template (server, templ, NULL, NULL, NULL);

    templates = mcp_server_list_resource_templates (server);
    g_assert_cmpuint (g_list_length (templates), ==, 1);
    g_list_free_full (templates, g_object_unref);
}

static void
test_server_add_prompt (void)
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpPrompt) prompt = NULL;
    GList *prompts;
    McpServerCapabilities *caps;

    server = mcp_server_new ("test-server", "1.0.0");
    prompt = mcp_prompt_new ("greeting", "Generates a greeting message");

    mcp_server_add_prompt (server, prompt, NULL, NULL, NULL);

    prompts = mcp_server_list_prompts (server);
    g_assert_cmpuint (g_list_length (prompts), ==, 1);
    g_list_free_full (prompts, g_object_unref);

    /* Check capability was enabled */
    caps = mcp_server_get_capabilities (server);
    g_assert_true (mcp_server_capabilities_get_prompts (caps));
}

static void
test_server_multiple_entities (void)
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpTool) tool1 = NULL;
    g_autoptr(McpTool) tool2 = NULL;
    g_autoptr(McpResource) res = NULL;
    g_autoptr(McpPrompt) prompt = NULL;
    GList *tools, *resources, *prompts;

    server = mcp_server_new ("multi-entity-server", "2.0.0");

    tool1 = mcp_tool_new ("tool1", "First tool");
    tool2 = mcp_tool_new ("tool2", "Second tool");
    res = mcp_resource_new ("file:///data.json", "data.json");
    prompt = mcp_prompt_new ("helper", "Helper prompt");

    mcp_server_add_tool (server, tool1, NULL, NULL, NULL);
    mcp_server_add_tool (server, tool2, NULL, NULL, NULL);
    mcp_server_add_resource (server, res, NULL, NULL, NULL);
    mcp_server_add_prompt (server, prompt, NULL, NULL, NULL);

    tools = mcp_server_list_tools (server);
    resources = mcp_server_list_resources (server);
    prompts = mcp_server_list_prompts (server);

    g_assert_cmpuint (g_list_length (tools), ==, 2);
    g_assert_cmpuint (g_list_length (resources), ==, 1);
    g_assert_cmpuint (g_list_length (prompts), ==, 1);

    g_list_free_full (tools, g_object_unref);
    g_list_free_full (resources, g_object_unref);
    g_list_free_full (prompts, g_object_unref);
}

static void
test_server_session_state (void)
{
    g_autoptr(McpServer) server = NULL;
    McpSessionState state;

    server = mcp_server_new ("test-server", "1.0.0");
    state = mcp_session_get_state (MCP_SESSION (server));

    /* Initially in disconnected state */
    g_assert_cmpint (state, ==, MCP_SESSION_STATE_DISCONNECTED);
}

/* ========================================================================== */
/* main                                                                       */
/* ========================================================================== */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* Tool result tests */
    g_test_add_func ("/mcp/tool-result/new", test_tool_result_new);
    g_test_add_func ("/mcp/tool-result/error", test_tool_result_error);
    g_test_add_func ("/mcp/tool-result/add-text", test_tool_result_add_text);
    g_test_add_func ("/mcp/tool-result/add-image", test_tool_result_add_image);

    /* Resource contents tests */
    g_test_add_func ("/mcp/resource-contents/text", test_resource_contents_text);
    g_test_add_func ("/mcp/resource-contents/blob", test_resource_contents_blob);

    /* Prompt message tests */
    g_test_add_func ("/mcp/prompt-message/new", test_prompt_message_new);
    g_test_add_func ("/mcp/prompt-message/add-text", test_prompt_message_add_text);
    g_test_add_func ("/mcp/prompt-message/add-image", test_prompt_message_add_image);

    /* Prompt result tests */
    g_test_add_func ("/mcp/prompt-result/new", test_prompt_result_new);
    g_test_add_func ("/mcp/prompt-result/messages", test_prompt_result_messages);
    g_test_add_func ("/mcp/prompt-result/to-json", test_prompt_result_to_json);

    /* Server tests */
    g_test_add_func ("/mcp/server/new", test_server_new);
    g_test_add_func ("/mcp/server/capabilities", test_server_capabilities);
    g_test_add_func ("/mcp/server/instructions", test_server_instructions);
    g_test_add_func ("/mcp/server/add-tool", test_server_add_tool);
    g_test_add_func ("/mcp/server/remove-tool", test_server_remove_tool);
    g_test_add_func ("/mcp/server/add-resource", test_server_add_resource);
    g_test_add_func ("/mcp/server/add-resource-template", test_server_add_resource_template);
    g_test_add_func ("/mcp/server/add-prompt", test_server_add_prompt);
    g_test_add_func ("/mcp/server/multiple-entities", test_server_multiple_entities);
    g_test_add_func ("/mcp/server/session-state", test_server_session_state);

    return g_test_run ();
}
