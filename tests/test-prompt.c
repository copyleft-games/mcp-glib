/*
 * test-prompt.c - Unit tests for McpPrompt and McpPromptArgument
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <glib.h>
#include "mcp.h"

/* ========================================================================== */
/* McpPromptArgument Tests                                                    */
/* ========================================================================== */

/*
 * Test basic argument creation
 */
static void
test_prompt_argument_new (void)
{
    McpPromptArgument *arg;

    arg = mcp_prompt_argument_new ("input", "User input text", TRUE);

    g_assert_nonnull (arg);
    g_assert_cmpstr (mcp_prompt_argument_get_name (arg), ==, "input");
    g_assert_cmpstr (mcp_prompt_argument_get_description (arg), ==, "User input text");
    g_assert_true (mcp_prompt_argument_get_required (arg));

    mcp_prompt_argument_free (arg);
}

/*
 * Test argument with NULL description
 */
static void
test_prompt_argument_null_description (void)
{
    McpPromptArgument *arg;

    arg = mcp_prompt_argument_new ("optional", NULL, FALSE);

    g_assert_nonnull (arg);
    g_assert_cmpstr (mcp_prompt_argument_get_name (arg), ==, "optional");
    g_assert_null (mcp_prompt_argument_get_description (arg));
    g_assert_false (mcp_prompt_argument_get_required (arg));

    mcp_prompt_argument_free (arg);
}

/*
 * Test argument copy
 */
static void
test_prompt_argument_copy (void)
{
    McpPromptArgument *original;
    McpPromptArgument *copy;

    original = mcp_prompt_argument_new ("test", "Test description", TRUE);
    copy = mcp_prompt_argument_copy (original);

    g_assert_nonnull (copy);
    g_assert_cmpstr (mcp_prompt_argument_get_name (copy), ==, mcp_prompt_argument_get_name (original));
    g_assert_cmpstr (mcp_prompt_argument_get_description (copy), ==, mcp_prompt_argument_get_description (original));
    g_assert_cmpint (mcp_prompt_argument_get_required (copy), ==, mcp_prompt_argument_get_required (original));

    mcp_prompt_argument_free (original);
    mcp_prompt_argument_free (copy);
}

/*
 * Test argument JSON serialization
 */
static void
test_prompt_argument_to_json (void)
{
    McpPromptArgument *arg;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    arg = mcp_prompt_argument_new ("topic", "The topic to discuss", TRUE);
    json = mcp_prompt_argument_to_json (arg);

    g_assert_nonnull (json);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (json));

    obj = json_node_get_object (json);

    g_assert_true (json_object_has_member (obj, "name"));
    g_assert_cmpstr (json_object_get_string_member (obj, "name"), ==, "topic");

    g_assert_true (json_object_has_member (obj, "description"));
    g_assert_cmpstr (json_object_get_string_member (obj, "description"), ==, "The topic to discuss");

    g_assert_true (json_object_has_member (obj, "required"));
    g_assert_true (json_object_get_boolean_member (obj, "required"));

    mcp_prompt_argument_free (arg);
}

/*
 * Test argument JSON serialization without optional fields
 */
static void
test_prompt_argument_to_json_minimal (void)
{
    McpPromptArgument *arg;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    arg = mcp_prompt_argument_new ("simple", NULL, FALSE);
    json = mcp_prompt_argument_to_json (arg);

    g_assert_nonnull (json);
    obj = json_node_get_object (json);

    g_assert_true (json_object_has_member (obj, "name"));
    g_assert_false (json_object_has_member (obj, "description"));
    g_assert_false (json_object_has_member (obj, "required"));

    mcp_prompt_argument_free (arg);
}

/*
 * Test argument JSON deserialization
 */
static void
test_prompt_argument_from_json (void)
{
    McpPromptArgument *arg;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str =
        "{"
        "  \"name\": \"context\","
        "  \"description\": \"Additional context\","
        "  \"required\": true"
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    arg = mcp_prompt_argument_new_from_json (root, &error);

    g_assert_no_error (error);
    g_assert_nonnull (arg);

    g_assert_cmpstr (mcp_prompt_argument_get_name (arg), ==, "context");
    g_assert_cmpstr (mcp_prompt_argument_get_description (arg), ==, "Additional context");
    g_assert_true (mcp_prompt_argument_get_required (arg));

    mcp_prompt_argument_free (arg);
}

/*
 * Test argument JSON deserialization with missing name (should fail)
 */
static void
test_prompt_argument_from_json_missing_name (void)
{
    McpPromptArgument *arg;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str = "{ \"description\": \"No name here\" }";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    arg = mcp_prompt_argument_new_from_json (root, &error);

    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_null (arg);
}

/* ========================================================================== */
/* McpPrompt Tests                                                            */
/* ========================================================================== */

/*
 * Test basic prompt creation
 */
static void
test_prompt_new (void)
{
    g_autoptr(McpPrompt) prompt = NULL;

    prompt = mcp_prompt_new ("greeting", "A greeting prompt");

    g_assert_nonnull (prompt);
    g_assert_cmpstr (mcp_prompt_get_name (prompt), ==, "greeting");
    g_assert_cmpstr (mcp_prompt_get_description (prompt), ==, "A greeting prompt");
    g_assert_null (mcp_prompt_get_title (prompt));
    g_assert_null (mcp_prompt_get_arguments (prompt));
}

/*
 * Test prompt with NULL description
 */
static void
test_prompt_new_null_description (void)
{
    g_autoptr(McpPrompt) prompt = NULL;

    prompt = mcp_prompt_new ("simple", NULL);

    g_assert_nonnull (prompt);
    g_assert_cmpstr (mcp_prompt_get_name (prompt), ==, "simple");
    g_assert_null (mcp_prompt_get_description (prompt));
}

/*
 * Test prompt properties
 */
static void
test_prompt_properties (void)
{
    g_autoptr(McpPrompt) prompt = NULL;

    prompt = mcp_prompt_new ("test", "Test prompt");

    /* Test title property */
    g_assert_null (mcp_prompt_get_title (prompt));
    mcp_prompt_set_title (prompt, "Test Prompt Title");
    g_assert_cmpstr (mcp_prompt_get_title (prompt), ==, "Test Prompt Title");

    /* Test changing name */
    mcp_prompt_set_name (prompt, "new-name");
    g_assert_cmpstr (mcp_prompt_get_name (prompt), ==, "new-name");

    /* Test changing description */
    mcp_prompt_set_description (prompt, "New description");
    g_assert_cmpstr (mcp_prompt_get_description (prompt), ==, "New description");
}

/*
 * Test adding arguments
 */
static void
test_prompt_arguments (void)
{
    g_autoptr(McpPrompt) prompt = NULL;
    GList *args;

    prompt = mcp_prompt_new ("templated", "A templated prompt");

    g_assert_null (mcp_prompt_get_arguments (prompt));

    /* Add arguments using convenience function */
    mcp_prompt_add_argument_full (prompt, "name", "User's name", TRUE);
    mcp_prompt_add_argument_full (prompt, "style", "Response style", FALSE);

    args = mcp_prompt_get_arguments (prompt);
    g_assert_nonnull (args);
    g_assert_cmpuint (g_list_length (args), ==, 2);

    /* Verify first argument */
    McpPromptArgument *arg1 = (McpPromptArgument *) g_list_nth_data (args, 0);
    g_assert_cmpstr (mcp_prompt_argument_get_name (arg1), ==, "name");
    g_assert_true (mcp_prompt_argument_get_required (arg1));

    /* Verify second argument */
    McpPromptArgument *arg2 = (McpPromptArgument *) g_list_nth_data (args, 1);
    g_assert_cmpstr (mcp_prompt_argument_get_name (arg2), ==, "style");
    g_assert_false (mcp_prompt_argument_get_required (arg2));

    /* Test clear */
    mcp_prompt_clear_arguments (prompt);
    g_assert_null (mcp_prompt_get_arguments (prompt));
}

/*
 * Test JSON serialization
 */
static void
test_prompt_to_json (void)
{
    g_autoptr(McpPrompt) prompt = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;
    JsonArray *args_array;

    prompt = mcp_prompt_new ("summarize", "Summarize text");
    mcp_prompt_set_title (prompt, "Text Summarizer");
    mcp_prompt_add_argument_full (prompt, "text", "Text to summarize", TRUE);
    mcp_prompt_add_argument_full (prompt, "length", "Summary length", FALSE);

    json = mcp_prompt_to_json (prompt);

    g_assert_nonnull (json);
    g_assert_true (JSON_NODE_HOLDS_OBJECT (json));

    obj = json_node_get_object (json);

    g_assert_true (json_object_has_member (obj, "name"));
    g_assert_cmpstr (json_object_get_string_member (obj, "name"), ==, "summarize");

    g_assert_true (json_object_has_member (obj, "title"));
    g_assert_cmpstr (json_object_get_string_member (obj, "title"), ==, "Text Summarizer");

    g_assert_true (json_object_has_member (obj, "description"));
    g_assert_cmpstr (json_object_get_string_member (obj, "description"), ==, "Summarize text");

    g_assert_true (json_object_has_member (obj, "arguments"));
    args_array = json_object_get_array_member (obj, "arguments");
    g_assert_cmpuint (json_array_get_length (args_array), ==, 2);
}

/*
 * Test JSON serialization without optional fields
 */
static void
test_prompt_to_json_minimal (void)
{
    g_autoptr(McpPrompt) prompt = NULL;
    g_autoptr(JsonNode) json = NULL;
    JsonObject *obj;

    prompt = mcp_prompt_new ("simple", NULL);

    json = mcp_prompt_to_json (prompt);

    g_assert_nonnull (json);
    obj = json_node_get_object (json);

    g_assert_true (json_object_has_member (obj, "name"));
    g_assert_false (json_object_has_member (obj, "title"));
    g_assert_false (json_object_has_member (obj, "description"));
    g_assert_false (json_object_has_member (obj, "arguments"));
}

/*
 * Test JSON deserialization
 */
static void
test_prompt_from_json (void)
{
    g_autoptr(McpPrompt) prompt = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;
    GList *args;

    const gchar *json_str =
        "{"
        "  \"name\": \"translate\","
        "  \"title\": \"Text Translator\","
        "  \"description\": \"Translate text between languages\","
        "  \"arguments\": ["
        "    { \"name\": \"text\", \"description\": \"Text to translate\", \"required\": true },"
        "    { \"name\": \"target_lang\", \"description\": \"Target language\", \"required\": true },"
        "    { \"name\": \"source_lang\", \"description\": \"Source language\" }"
        "  ]"
        "}";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    prompt = mcp_prompt_new_from_json (root, &error);

    g_assert_no_error (error);
    g_assert_nonnull (prompt);

    g_assert_cmpstr (mcp_prompt_get_name (prompt), ==, "translate");
    g_assert_cmpstr (mcp_prompt_get_title (prompt), ==, "Text Translator");
    g_assert_cmpstr (mcp_prompt_get_description (prompt), ==, "Translate text between languages");

    args = mcp_prompt_get_arguments (prompt);
    g_assert_nonnull (args);
    g_assert_cmpuint (g_list_length (args), ==, 3);

    McpPromptArgument *arg1 = (McpPromptArgument *) g_list_nth_data (args, 0);
    g_assert_cmpstr (mcp_prompt_argument_get_name (arg1), ==, "text");
    g_assert_true (mcp_prompt_argument_get_required (arg1));

    McpPromptArgument *arg3 = (McpPromptArgument *) g_list_nth_data (args, 2);
    g_assert_cmpstr (mcp_prompt_argument_get_name (arg3), ==, "source_lang");
    g_assert_false (mcp_prompt_argument_get_required (arg3));
}

/*
 * Test JSON deserialization with missing name (should fail)
 */
static void
test_prompt_from_json_missing_name (void)
{
    g_autoptr(McpPrompt) prompt = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) error = NULL;
    JsonNode *root;

    const gchar *json_str = "{ \"description\": \"Missing name\" }";

    parser = json_parser_new ();
    g_assert_true (json_parser_load_from_data (parser, json_str, -1, &error));
    g_assert_no_error (error);

    root = json_parser_get_root (parser);
    prompt = mcp_prompt_new_from_json (root, &error);

    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_null (prompt);
}

/*
 * Test round-trip JSON serialization
 */
static void
test_prompt_json_roundtrip (void)
{
    g_autoptr(McpPrompt) original = NULL;
    g_autoptr(McpPrompt) restored = NULL;
    g_autoptr(JsonNode) json = NULL;
    g_autoptr(GError) error = NULL;
    GList *orig_args, *rest_args;

    original = mcp_prompt_new ("analyze", "Analyze data");
    mcp_prompt_set_title (original, "Data Analyzer");
    mcp_prompt_add_argument_full (original, "data", "Input data", TRUE);
    mcp_prompt_add_argument_full (original, "format", "Output format", FALSE);

    json = mcp_prompt_to_json (original);
    restored = mcp_prompt_new_from_json (json, &error);

    g_assert_no_error (error);
    g_assert_nonnull (restored);

    g_assert_cmpstr (mcp_prompt_get_name (original), ==, mcp_prompt_get_name (restored));
    g_assert_cmpstr (mcp_prompt_get_title (original), ==, mcp_prompt_get_title (restored));
    g_assert_cmpstr (mcp_prompt_get_description (original), ==, mcp_prompt_get_description (restored));

    orig_args = mcp_prompt_get_arguments (original);
    rest_args = mcp_prompt_get_arguments (restored);

    g_assert_cmpuint (g_list_length (orig_args), ==, g_list_length (rest_args));

    /* Verify arguments match */
    GList *o = orig_args, *r = rest_args;
    while (o != NULL && r != NULL)
    {
        McpPromptArgument *oa = (McpPromptArgument *) o->data;
        McpPromptArgument *ra = (McpPromptArgument *) r->data;

        g_assert_cmpstr (mcp_prompt_argument_get_name (oa), ==, mcp_prompt_argument_get_name (ra));
        g_assert_cmpstr (mcp_prompt_argument_get_description (oa), ==, mcp_prompt_argument_get_description (ra));
        g_assert_cmpint (mcp_prompt_argument_get_required (oa), ==, mcp_prompt_argument_get_required (ra));

        o = o->next;
        r = r->next;
    }
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    /* McpPromptArgument tests */
    g_test_add_func ("/mcp/prompt-argument/new", test_prompt_argument_new);
    g_test_add_func ("/mcp/prompt-argument/null-description", test_prompt_argument_null_description);
    g_test_add_func ("/mcp/prompt-argument/copy", test_prompt_argument_copy);
    g_test_add_func ("/mcp/prompt-argument/to-json", test_prompt_argument_to_json);
    g_test_add_func ("/mcp/prompt-argument/to-json-minimal", test_prompt_argument_to_json_minimal);
    g_test_add_func ("/mcp/prompt-argument/from-json", test_prompt_argument_from_json);
    g_test_add_func ("/mcp/prompt-argument/from-json-missing-name", test_prompt_argument_from_json_missing_name);

    /* McpPrompt tests */
    g_test_add_func ("/mcp/prompt/new", test_prompt_new);
    g_test_add_func ("/mcp/prompt/new-null-description", test_prompt_new_null_description);
    g_test_add_func ("/mcp/prompt/properties", test_prompt_properties);
    g_test_add_func ("/mcp/prompt/arguments", test_prompt_arguments);
    g_test_add_func ("/mcp/prompt/to-json", test_prompt_to_json);
    g_test_add_func ("/mcp/prompt/to-json-minimal", test_prompt_to_json_minimal);
    g_test_add_func ("/mcp/prompt/from-json", test_prompt_from_json);
    g_test_add_func ("/mcp/prompt/from-json-missing-name", test_prompt_from_json_missing_name);
    g_test_add_func ("/mcp/prompt/json-roundtrip", test_prompt_json_roundtrip);

    return g_test_run ();
}
