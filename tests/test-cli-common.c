/*
 * test-cli-common.c - Unit tests for CLI tool common utilities
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Tests the shared utilities in tools/mcp-common.c used by all CLI tools.
 */

#include <glib.h>
#include <json-glib/json-glib.h>
#include "mcp.h"
#include "mcp-common.h"

/* ========================================================================== */
/* Option Tests                                                                */
/* ========================================================================== */

/*
 * test_get_common_options_returns_array:
 *
 * Verify that mcp_cli_get_common_options() returns a valid GOptionEntry array.
 */
static void
test_get_common_options_returns_array (void)
{
    GOptionEntry *entries;
    gint count;

    entries = mcp_cli_get_common_options ();
    g_assert_nonnull (entries);

    /* Count entries (terminated by NULL name) */
    count = 0;
    while (entries[count].long_name != NULL)
    {
        count++;
    }

    /* Should have at least the 8 common options */
    g_assert_cmpint (count, >=, 8);

    /* Verify some expected entries exist */
    g_assert_cmpstr (entries[0].long_name, ==, "stdio");
    g_assert_cmpint (entries[0].short_name, ==, 's');
}

/*
 * test_reset_options_restores_defaults:
 *
 * Verify that mcp_cli_reset_options() restores default values.
 */
static void
test_reset_options_restores_defaults (void)
{
    /* Set some non-default values */
    mcp_cli_opt_timeout = 999;
    mcp_cli_opt_json = TRUE;
    mcp_cli_opt_quiet = TRUE;
    mcp_cli_opt_license = TRUE;

    /* Reset */
    mcp_cli_reset_options ();

    /* Verify defaults restored */
    g_assert_cmpint (mcp_cli_opt_timeout, ==, 30);
    g_assert_false (mcp_cli_opt_json);
    g_assert_false (mcp_cli_opt_quiet);
    g_assert_false (mcp_cli_opt_license);
}

/*
 * test_reset_options_frees_strings:
 *
 * Verify that mcp_cli_reset_options() frees allocated strings.
 */
static void
test_reset_options_frees_strings (void)
{
    /* Set string options (allocate new strings) */
    mcp_cli_opt_stdio = g_strdup ("./test-server");
    mcp_cli_opt_http = g_strdup ("https://example.com");
    mcp_cli_opt_ws = g_strdup ("wss://example.com");
    mcp_cli_opt_token = g_strdup ("secret-token");

    /* Reset should free and NULL these */
    mcp_cli_reset_options ();

    /* All strings should be NULL after reset */
    g_assert_null (mcp_cli_opt_stdio);
    g_assert_null (mcp_cli_opt_http);
    g_assert_null (mcp_cli_opt_ws);
    g_assert_null (mcp_cli_opt_token);
}

/* ========================================================================== */
/* Transport Creation Tests                                                    */
/* ========================================================================== */

/*
 * test_create_transport_no_transport_error:
 *
 * Verify that mcp_cli_create_transport() fails when no transport is specified.
 */
static void
test_create_transport_no_transport_error (void)
{
    g_autoptr(GError) error = NULL;
    McpTransport *transport;

    /* Ensure all transport options are NULL */
    mcp_cli_reset_options ();

    transport = mcp_cli_create_transport (&error);

    g_assert_null (transport);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_true (g_str_has_prefix (error->message, "No transport"));
}

/*
 * test_create_transport_multiple_transports_error:
 *
 * Verify that mcp_cli_create_transport() fails when multiple transports specified.
 */
static void
test_create_transport_multiple_transports_error (void)
{
    g_autoptr(GError) error = NULL;
    McpTransport *transport;

    mcp_cli_reset_options ();

    /* Set multiple transport options */
    mcp_cli_opt_stdio = g_strdup ("./server");
    mcp_cli_opt_http = g_strdup ("https://example.com");

    transport = mcp_cli_create_transport (&error);

    g_assert_null (transport);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_true (g_str_has_prefix (error->message, "Multiple transports"));

    /* Clean up */
    mcp_cli_reset_options ();
}

/*
 * test_create_transport_invalid_stdio_command:
 *
 * Verify that mcp_cli_create_transport() fails with invalid stdio command.
 */
static void
test_create_transport_invalid_stdio_command (void)
{
    g_autoptr(GError) error = NULL;
    McpTransport *transport;

    mcp_cli_reset_options ();

    /* Set an invalid command (non-existent path) */
    mcp_cli_opt_stdio = g_strdup ("/nonexistent/path/to/binary");

    transport = mcp_cli_create_transport (&error);

    /* Should fail because command doesn't exist */
    g_assert_null (transport);
    g_assert_nonnull (error);

    /* Clean up */
    mcp_cli_reset_options ();
}

/* ========================================================================== */
/* JSON Argument Parsing Tests                                                 */
/* ========================================================================== */

/*
 * test_parse_json_args_valid_object:
 *
 * Verify that valid JSON object is parsed correctly.
 */
static void
test_parse_json_args_valid_object (void)
{
    g_autoptr(GError) error = NULL;
    JsonObject *obj;

    obj = mcp_cli_parse_json_args ("{\"a\": 5, \"b\": \"hello\"}", &error);

    g_assert_no_error (error);
    g_assert_nonnull (obj);
    g_assert_true (json_object_has_member (obj, "a"));
    g_assert_true (json_object_has_member (obj, "b"));
    g_assert_cmpint (json_object_get_int_member (obj, "a"), ==, 5);
    g_assert_cmpstr (json_object_get_string_member (obj, "b"), ==, "hello");

    json_object_unref (obj);
}

/*
 * test_parse_json_args_empty_string:
 *
 * Verify that empty string returns empty object.
 */
static void
test_parse_json_args_empty_string (void)
{
    g_autoptr(GError) error = NULL;
    JsonObject *obj;

    obj = mcp_cli_parse_json_args ("", &error);

    g_assert_no_error (error);
    g_assert_nonnull (obj);
    g_assert_cmpint (json_object_get_size (obj), ==, 0);

    json_object_unref (obj);
}

/*
 * test_parse_json_args_null_returns_empty_object:
 *
 * Verify that NULL input returns empty object.
 */
static void
test_parse_json_args_null_returns_empty_object (void)
{
    g_autoptr(GError) error = NULL;
    JsonObject *obj;

    obj = mcp_cli_parse_json_args (NULL, &error);

    g_assert_no_error (error);
    g_assert_nonnull (obj);
    g_assert_cmpint (json_object_get_size (obj), ==, 0);

    json_object_unref (obj);
}

/*
 * test_parse_json_args_invalid_json_error:
 *
 * Verify that invalid JSON string produces an error.
 */
static void
test_parse_json_args_invalid_json_error (void)
{
    g_autoptr(GError) error = NULL;
    JsonObject *obj;

    obj = mcp_cli_parse_json_args ("{invalid json}", &error);

    g_assert_null (obj);
    g_assert_nonnull (error);
}

/*
 * test_parse_json_args_array_error:
 *
 * Verify that JSON array (not object) produces an error.
 */
static void
test_parse_json_args_array_error (void)
{
    g_autoptr(GError) error = NULL;
    JsonObject *obj;

    obj = mcp_cli_parse_json_args ("[1, 2, 3]", &error);

    g_assert_null (obj);
    g_assert_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS);
    g_assert_true (g_str_has_prefix (error->message, "Arguments must be"));
}

/*
 * test_parse_json_args_nested_object:
 *
 * Verify that nested JSON objects are parsed correctly.
 */
static void
test_parse_json_args_nested_object (void)
{
    g_autoptr(GError) error = NULL;
    JsonObject *obj;
    JsonObject *nested;

    obj = mcp_cli_parse_json_args ("{\"outer\": {\"inner\": 42}}", &error);

    g_assert_no_error (error);
    g_assert_nonnull (obj);
    g_assert_true (json_object_has_member (obj, "outer"));

    nested = json_object_get_object_member (obj, "outer");
    g_assert_nonnull (nested);
    g_assert_cmpint (json_object_get_int_member (nested, "inner"), ==, 42);

    json_object_unref (obj);
}

/* ========================================================================== */
/* Prompt Argument Parsing Tests                                               */
/* ========================================================================== */

/*
 * test_parse_prompt_args_single_pair:
 *
 * Verify that single key=value pair is parsed correctly.
 */
static void
test_parse_prompt_args_single_pair (void)
{
    gchar *args[] = { "name=Alice" };
    GHashTable *table;

    table = mcp_cli_parse_prompt_args (args, 1);

    g_assert_nonnull (table);
    g_assert_cmpint (g_hash_table_size (table), ==, 1);
    g_assert_cmpstr (g_hash_table_lookup (table, "name"), ==, "Alice");

    g_hash_table_unref (table);
}

/*
 * test_parse_prompt_args_multiple_pairs:
 *
 * Verify that multiple key=value pairs are parsed correctly.
 */
static void
test_parse_prompt_args_multiple_pairs (void)
{
    gchar *args[] = { "name=Alice", "age=30", "city=NYC" };
    GHashTable *table;

    table = mcp_cli_parse_prompt_args (args, 3);

    g_assert_nonnull (table);
    g_assert_cmpint (g_hash_table_size (table), ==, 3);
    g_assert_cmpstr (g_hash_table_lookup (table, "name"), ==, "Alice");
    g_assert_cmpstr (g_hash_table_lookup (table, "age"), ==, "30");
    g_assert_cmpstr (g_hash_table_lookup (table, "city"), ==, "NYC");

    g_hash_table_unref (table);
}

/*
 * test_parse_prompt_args_no_equals_empty_value:
 *
 * Verify that key without '=' gets empty string value.
 */
static void
test_parse_prompt_args_no_equals_empty_value (void)
{
    gchar *args[] = { "flag" };
    GHashTable *table;

    table = mcp_cli_parse_prompt_args (args, 1);

    g_assert_nonnull (table);
    g_assert_cmpint (g_hash_table_size (table), ==, 1);
    g_assert_cmpstr (g_hash_table_lookup (table, "flag"), ==, "");

    g_hash_table_unref (table);
}

/*
 * test_parse_prompt_args_empty_value:
 *
 * Verify that key= (empty value after equals) is parsed correctly.
 */
static void
test_parse_prompt_args_empty_value (void)
{
    gchar *args[] = { "empty=" };
    GHashTable *table;

    table = mcp_cli_parse_prompt_args (args, 1);

    g_assert_nonnull (table);
    g_assert_cmpint (g_hash_table_size (table), ==, 1);
    g_assert_cmpstr (g_hash_table_lookup (table, "empty"), ==, "");

    g_hash_table_unref (table);
}

/*
 * test_parse_prompt_args_value_with_equals:
 *
 * Verify that value containing '=' is parsed correctly.
 */
static void
test_parse_prompt_args_value_with_equals (void)
{
    gchar *args[] = { "equation=a=b+c" };
    GHashTable *table;

    table = mcp_cli_parse_prompt_args (args, 1);

    g_assert_nonnull (table);
    g_assert_cmpint (g_hash_table_size (table), ==, 1);
    g_assert_cmpstr (g_hash_table_lookup (table, "equation"), ==, "a=b+c");

    g_hash_table_unref (table);
}

/*
 * test_parse_prompt_args_zero_args_empty_table:
 *
 * Verify that zero arguments returns empty hash table.
 */
static void
test_parse_prompt_args_zero_args_empty_table (void)
{
    GHashTable *table;

    table = mcp_cli_parse_prompt_args (NULL, 0);

    g_assert_nonnull (table);
    g_assert_cmpint (g_hash_table_size (table), ==, 0);

    g_hash_table_unref (table);
}

/* ========================================================================== */
/* Main Entry Point                                                            */
/* ========================================================================== */

int
main (int    argc,
      char **argv)
{
    g_test_init (&argc, &argv, NULL);

    /* Option tests */
    g_test_add_func ("/cli/options/get-common-options",
                     test_get_common_options_returns_array);
    g_test_add_func ("/cli/options/reset-restores-defaults",
                     test_reset_options_restores_defaults);
    g_test_add_func ("/cli/options/reset-frees-strings",
                     test_reset_options_frees_strings);

    /* Transport creation tests */
    g_test_add_func ("/cli/transport/no-transport-error",
                     test_create_transport_no_transport_error);
    g_test_add_func ("/cli/transport/multiple-transports-error",
                     test_create_transport_multiple_transports_error);
    g_test_add_func ("/cli/transport/invalid-stdio-command",
                     test_create_transport_invalid_stdio_command);

    /* JSON argument parsing tests */
    g_test_add_func ("/cli/json-args/valid-object",
                     test_parse_json_args_valid_object);
    g_test_add_func ("/cli/json-args/empty-string",
                     test_parse_json_args_empty_string);
    g_test_add_func ("/cli/json-args/null-returns-empty",
                     test_parse_json_args_null_returns_empty_object);
    g_test_add_func ("/cli/json-args/invalid-json-error",
                     test_parse_json_args_invalid_json_error);
    g_test_add_func ("/cli/json-args/array-error",
                     test_parse_json_args_array_error);
    g_test_add_func ("/cli/json-args/nested-object",
                     test_parse_json_args_nested_object);

    /* Prompt argument parsing tests */
    g_test_add_func ("/cli/prompt-args/single-pair",
                     test_parse_prompt_args_single_pair);
    g_test_add_func ("/cli/prompt-args/multiple-pairs",
                     test_parse_prompt_args_multiple_pairs);
    g_test_add_func ("/cli/prompt-args/no-equals-empty-value",
                     test_parse_prompt_args_no_equals_empty_value);
    g_test_add_func ("/cli/prompt-args/empty-value",
                     test_parse_prompt_args_empty_value);
    g_test_add_func ("/cli/prompt-args/value-with-equals",
                     test_parse_prompt_args_value_with_equals);
    g_test_add_func ("/cli/prompt-args/zero-args-empty-table",
                     test_parse_prompt_args_zero_args_empty_table);

    return g_test_run ();
}
