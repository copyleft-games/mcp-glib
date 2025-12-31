/*
 * filesystem-server.c - Filesystem MCP Server Example
 *
 * This example demonstrates a resource-oriented MCP server that:
 *   - Exposes files in a directory as resources
 *   - Uses resource templates for dynamic file access
 *   - Supports reading files by path
 *   - Includes a file listing tool
 *
 * To run:
 *   ./filesystem-server [directory]
 *
 * Default directory is the current working directory.
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "mcp.h"

static GMainLoop *main_loop = NULL;
static gchar *base_directory = NULL;

/* ========================================================================== */
/* Helper Functions                                                           */
/* ========================================================================== */

/*
 * get_mime_type:
 * @filename: the file name
 *
 * Determines the MIME type based on file extension.
 *
 * Returns: the MIME type string
 */
static const gchar *
get_mime_type (const gchar *filename)
{
    if (g_str_has_suffix (filename, ".txt"))
        return "text/plain";
    if (g_str_has_suffix (filename, ".md"))
        return "text/markdown";
    if (g_str_has_suffix (filename, ".c") || g_str_has_suffix (filename, ".h"))
        return "text/x-c";
    if (g_str_has_suffix (filename, ".py"))
        return "text/x-python";
    if (g_str_has_suffix (filename, ".json"))
        return "application/json";
    if (g_str_has_suffix (filename, ".xml"))
        return "application/xml";
    if (g_str_has_suffix (filename, ".html") || g_str_has_suffix (filename, ".htm"))
        return "text/html";
    if (g_str_has_suffix (filename, ".css"))
        return "text/css";
    if (g_str_has_suffix (filename, ".js"))
        return "application/javascript";

    return "application/octet-stream";
}

/*
 * is_path_safe:
 * @path: the requested path
 *
 * Checks if the path is safe (no path traversal attacks).
 *
 * Returns: TRUE if safe, FALSE if suspicious
 */
static gboolean
is_path_safe (const gchar *path)
{
    /* Reject absolute paths */
    if (g_path_is_absolute (path))
    {
        return FALSE;
    }

    /* Reject path traversal attempts */
    if (strstr (path, "..") != NULL)
    {
        return FALSE;
    }

    return TRUE;
}

/*
 * resolve_file_uri:
 * @uri: the file URI (file:///path)
 *
 * Resolves a file URI to an absolute path within the base directory.
 *
 * Returns: (transfer full) (nullable): the absolute path, or NULL if invalid
 */
static gchar *
resolve_file_uri (const gchar *uri)
{
    const gchar *path;

    if (!g_str_has_prefix (uri, "file:///"))
    {
        return NULL;
    }

    path = uri + 8;  /* Skip "file:///" */

    if (!is_path_safe (path))
    {
        return NULL;
    }

    return g_build_filename (base_directory, path, NULL);
}

/* ========================================================================== */
/* Resource Handler                                                           */
/* ========================================================================== */

/*
 * file_resource_handler:
 * @server: the MCP server
 * @uri: the resource URI
 * @user_data: unused
 *
 * Reads a file and returns its contents.
 *
 * Returns: (transfer full) (element-type McpResourceContents): list of contents
 */
static GList *
file_resource_handler (McpServer   *server,
                       const gchar *uri,
                       gpointer     user_data)
{
    g_autofree gchar *path = NULL;
    g_autofree gchar *contents = NULL;
    gsize length;
    g_autoptr(GError) error = NULL;
    McpResourceContents *resource_contents;
    const gchar *mime_type;

    path = resolve_file_uri (uri);
    if (path == NULL)
    {
        g_warning ("Invalid file URI: %s", uri);
        return NULL;
    }

    /* Read the file */
    if (!g_file_get_contents (path, &contents, &length, &error))
    {
        g_warning ("Failed to read %s: %s", path, error->message);
        return NULL;
    }

    /* Determine MIME type */
    mime_type = get_mime_type (path);

    /* Create the resource contents */
    if (g_str_has_prefix (mime_type, "text/") ||
        g_strcmp0 (mime_type, "application/json") == 0 ||
        g_strcmp0 (mime_type, "application/xml") == 0 ||
        g_strcmp0 (mime_type, "application/javascript") == 0)
    {
        /* Text content */
        resource_contents = mcp_resource_contents_new_text (uri, contents, mime_type);
    }
    else
    {
        /* Binary content - encode as base64 */
        g_autofree gchar *base64 = g_base64_encode ((guchar *) contents, length);
        resource_contents = mcp_resource_contents_new_blob (uri, base64, mime_type);
    }

    return g_list_append (NULL, resource_contents);
}

/* ========================================================================== */
/* Tool Handler                                                               */
/* ========================================================================== */

/*
 * list_files_handler:
 * @server: the MCP server
 * @name: tool name
 * @arguments: JSON arguments (path: optional subdirectory)
 * @user_data: unused
 *
 * Lists files in the base directory or a subdirectory.
 *
 * Returns: (transfer full): the tool result with file listing
 */
static McpToolResult *
list_files_handler (McpServer   *server,
                    const gchar *name,
                    JsonObject  *arguments,
                    gpointer     user_data)
{
    McpToolResult *result;
    g_autofree gchar *dir_path = NULL;
    g_autoptr(GDir) dir = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = NULL;
    const gchar *entry_name;
    const gchar *subpath = NULL;

    /* Get optional subdirectory */
    if (arguments != NULL && json_object_has_member (arguments, "path"))
    {
        subpath = json_object_get_string_member (arguments, "path");
        if (subpath != NULL && !is_path_safe (subpath))
        {
            result = mcp_tool_result_new (TRUE);
            mcp_tool_result_add_text (result, "Invalid path");
            return result;
        }
    }

    /* Build directory path */
    if (subpath != NULL && subpath[0] != '\0')
    {
        dir_path = g_build_filename (base_directory, subpath, NULL);
    }
    else
    {
        dir_path = g_strdup (base_directory);
    }

    /* Open directory */
    dir = g_dir_open (dir_path, 0, &error);
    if (dir == NULL)
    {
        result = mcp_tool_result_new (TRUE);
        mcp_tool_result_add_text (result, error->message);
        return result;
    }

    /* Build file listing */
    output = g_string_new ("Files:\n");

    while ((entry_name = g_dir_read_name (dir)) != NULL)
    {
        g_autofree gchar *full_path = g_build_filename (dir_path, entry_name, NULL);
        gboolean is_dir = g_file_test (full_path, G_FILE_TEST_IS_DIR);

        g_string_append_printf (output, "  %s%s\n",
                                entry_name,
                                is_dir ? "/" : "");
    }

    result = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_text (result, output->str);

    return result;
}

/*
 * read_file_handler:
 * @server: the MCP server
 * @name: tool name
 * @arguments: JSON arguments (path: file path relative to base)
 * @user_data: unused
 *
 * Reads and returns file contents as a tool result.
 *
 * Returns: (transfer full): the tool result with file contents
 */
static McpToolResult *
read_file_handler (McpServer   *server,
                   const gchar *name,
                   JsonObject  *arguments,
                   gpointer     user_data)
{
    McpToolResult *result;
    g_autofree gchar *file_path = NULL;
    g_autofree gchar *contents = NULL;
    g_autoptr(GError) error = NULL;
    const gchar *path;

    if (arguments == NULL || !json_object_has_member (arguments, "path"))
    {
        result = mcp_tool_result_new (TRUE);
        mcp_tool_result_add_text (result, "Missing required argument 'path'");
        return result;
    }

    path = json_object_get_string_member (arguments, "path");
    if (!is_path_safe (path))
    {
        result = mcp_tool_result_new (TRUE);
        mcp_tool_result_add_text (result, "Invalid path");
        return result;
    }

    file_path = g_build_filename (base_directory, path, NULL);

    if (!g_file_get_contents (file_path, &contents, NULL, &error))
    {
        result = mcp_tool_result_new (TRUE);
        mcp_tool_result_add_text (result, error->message);
        return result;
    }

    result = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_text (result, contents);

    return result;
}

/* ========================================================================== */
/* Server Setup                                                               */
/* ========================================================================== */

static void
on_client_disconnected (McpServer *server,
                        gpointer   user_data)
{
    g_message ("Client disconnected, shutting down");
    g_main_loop_quit (main_loop);
}

static void
setup_server (McpServer *server)
{
    g_autoptr(McpResourceTemplate) file_template = NULL;
    g_autoptr(McpTool) list_tool = NULL;
    g_autoptr(McpTool) read_tool = NULL;

    /* Create a resource template for file access */
    file_template = mcp_resource_template_new ("file:///{path}", "File Access");
    mcp_resource_template_set_description (file_template, "Access files in the served directory");
    mcp_resource_template_set_mime_type (file_template, "application/octet-stream");
    mcp_server_add_resource_template (server, file_template,
                                       file_resource_handler, NULL, NULL);

    /* Add list_files tool */
    list_tool = mcp_tool_new ("list_files", "List files in the directory");
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonNode) schema = NULL;

        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "type");
        json_builder_add_string_value (builder, "object");
        json_builder_set_member_name (builder, "properties");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "path");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "type");
        json_builder_add_string_value (builder, "string");
        json_builder_set_member_name (builder, "description");
        json_builder_add_string_value (builder, "Optional subdirectory path");
        json_builder_end_object (builder);
        json_builder_end_object (builder);
        json_builder_end_object (builder);

        schema = json_builder_get_root (builder);
        mcp_tool_set_input_schema (list_tool, schema);
    }
    mcp_server_add_tool (server, list_tool, list_files_handler, NULL, NULL);

    /* Add read_file tool */
    read_tool = mcp_tool_new ("read_file", "Read a file's contents");
    {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        g_autoptr(JsonNode) schema = NULL;

        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "type");
        json_builder_add_string_value (builder, "object");
        json_builder_set_member_name (builder, "properties");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "path");
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "type");
        json_builder_add_string_value (builder, "string");
        json_builder_set_member_name (builder, "description");
        json_builder_add_string_value (builder, "File path relative to base directory");
        json_builder_end_object (builder);
        json_builder_end_object (builder);
        json_builder_set_member_name (builder, "required");
        json_builder_begin_array (builder);
        json_builder_add_string_value (builder, "path");
        json_builder_end_array (builder);
        json_builder_end_object (builder);

        schema = json_builder_get_root (builder);
        mcp_tool_set_input_schema (read_tool, schema);
    }
    mcp_server_add_tool (server, read_tool, read_file_handler, NULL, NULL);
}

/* ========================================================================== */
/* Main Entry Point                                                           */
/* ========================================================================== */

int
main (int   argc,
      char *argv[])
{
    g_autoptr(McpServer) server = NULL;
    g_autoptr(McpStdioTransport) transport = NULL;
    g_autofree gchar *instructions = NULL;

    /* Get base directory from command line or use current */
    if (argc > 1)
    {
        base_directory = g_strdup (argv[1]);
    }
    else
    {
        base_directory = g_get_current_dir ();
    }

    /* Verify directory exists */
    if (!g_file_test (base_directory, G_FILE_TEST_IS_DIR))
    {
        g_printerr ("Error: %s is not a directory\n", base_directory);
        g_free (base_directory);
        return 1;
    }

    main_loop = g_main_loop_new (NULL, FALSE);

    /* Create server */
    server = mcp_server_new ("filesystem-server", "1.0.0");

    instructions = g_strdup_printf (
        "This server provides access to files in: %s\n\n"
        "Available tools:\n"
        "- list_files(path?): List files in directory\n"
        "- read_file(path): Read file contents\n\n"
        "Resources can be accessed via file:///{path} URIs.",
        base_directory);
    mcp_server_set_instructions (server, instructions);

    g_signal_connect (server, "client-disconnected",
                      G_CALLBACK (on_client_disconnected), NULL);

    /* Set up tools and resources */
    setup_server (server);

    /* Set up transport and start */
    transport = mcp_stdio_transport_new ();
    mcp_server_set_transport (server, MCP_TRANSPORT (transport));

    g_message ("Starting Filesystem MCP Server for: %s", base_directory);
    mcp_server_start_async (server, NULL, NULL, NULL);

    g_main_loop_run (main_loop);

    /* Cleanup */
    g_main_loop_unref (main_loop);
    g_free (base_directory);
    g_message ("Server shut down");

    return 0;
}
