/*
 * calculator-server.c - Calculator MCP Server Example
 *
 * This example demonstrates a more complete MCP server with:
 *   - Multiple tools (add, subtract, multiply, divide, sqrt, power)
 *   - Error handling (division by zero, negative sqrt)
 *   - Complex JSON schemas for input validation
 *
 * To run:
 *   ./calculator-server
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <mcp/mcp.h>
#include <math.h>

static GMainLoop *main_loop = NULL;

/* ========================================================================== */
/* Helper Functions                                                           */
/* ========================================================================== */

/*
 * create_number_result:
 * @value: the numeric result
 *
 * Creates a tool result containing a numeric value as text.
 *
 * Returns: (transfer full): the tool result
 */
static McpToolResult *
create_number_result (gdouble value)
{
    McpToolResult *result;
    g_autofree gchar *text = NULL;

    /* Format the result nicely - use integer format if whole number */
    if (value == floor (value) && fabs (value) < 1e15)
    {
        text = g_strdup_printf ("%.0f", value);
    }
    else
    {
        text = g_strdup_printf ("%g", value);
    }

    result = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_text (result, text);

    return result;
}

/*
 * create_error_result:
 * @message: the error message
 *
 * Creates a tool result indicating an error.
 *
 * Returns: (transfer full): the error result
 */
static McpToolResult *
create_error_result (const gchar *message)
{
    McpToolResult *result;

    result = mcp_tool_result_new (TRUE);
    mcp_tool_result_add_text (result, message);

    return result;
}

/*
 * get_number:
 * @arguments: the JSON arguments object
 * @name: the argument name
 * @value: (out): the numeric value
 *
 * Extracts a numeric value from the arguments object.
 *
 * Returns: TRUE if successful, FALSE if missing or invalid
 */
static gboolean
get_number (JsonObject   *arguments,
            const gchar  *name,
            gdouble      *value)
{
    if (arguments == NULL || !json_object_has_member (arguments, name))
    {
        return FALSE;
    }

    *value = json_object_get_double_member (arguments, name);
    return TRUE;
}

/* ========================================================================== */
/* Tool Handlers                                                              */
/* ========================================================================== */

static McpToolResult *
add_handler (McpServer   *server,
             const gchar *name,
             JsonObject  *arguments,
             gpointer     user_data)
{
    gdouble a, b;

    if (!get_number (arguments, "a", &a) || !get_number (arguments, "b", &b))
    {
        return create_error_result ("Missing required arguments 'a' and 'b'");
    }

    return create_number_result (a + b);
}

static McpToolResult *
subtract_handler (McpServer   *server,
                  const gchar *name,
                  JsonObject  *arguments,
                  gpointer     user_data)
{
    gdouble a, b;

    if (!get_number (arguments, "a", &a) || !get_number (arguments, "b", &b))
    {
        return create_error_result ("Missing required arguments 'a' and 'b'");
    }

    return create_number_result (a - b);
}

static McpToolResult *
multiply_handler (McpServer   *server,
                  const gchar *name,
                  JsonObject  *arguments,
                  gpointer     user_data)
{
    gdouble a, b;

    if (!get_number (arguments, "a", &a) || !get_number (arguments, "b", &b))
    {
        return create_error_result ("Missing required arguments 'a' and 'b'");
    }

    return create_number_result (a * b);
}

static McpToolResult *
divide_handler (McpServer   *server,
                const gchar *name,
                JsonObject  *arguments,
                gpointer     user_data)
{
    gdouble a, b;

    if (!get_number (arguments, "a", &a) || !get_number (arguments, "b", &b))
    {
        return create_error_result ("Missing required arguments 'a' and 'b'");
    }

    if (b == 0)
    {
        return create_error_result ("Division by zero");
    }

    return create_number_result (a / b);
}

static McpToolResult *
sqrt_handler (McpServer   *server,
              const gchar *name,
              JsonObject  *arguments,
              gpointer     user_data)
{
    gdouble value;

    if (!get_number (arguments, "value", &value))
    {
        return create_error_result ("Missing required argument 'value'");
    }

    if (value < 0)
    {
        return create_error_result ("Cannot compute square root of negative number");
    }

    return create_number_result (sqrt (value));
}

static McpToolResult *
power_handler (McpServer   *server,
               const gchar *name,
               JsonObject  *arguments,
               gpointer     user_data)
{
    gdouble base, exponent;

    if (!get_number (arguments, "base", &base) ||
        !get_number (arguments, "exponent", &exponent))
    {
        return create_error_result ("Missing required arguments 'base' and 'exponent'");
    }

    return create_number_result (pow (base, exponent));
}

/* ========================================================================== */
/* Tool Schema Builders                                                       */
/* ========================================================================== */

/*
 * create_binary_op_schema:
 *
 * Creates a JSON schema for a binary operation (two numbers).
 *
 * Returns: (transfer full): the schema node
 */
static JsonNode *
create_binary_op_schema (void)
{
    g_autoptr(JsonBuilder) builder = json_builder_new ();

    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "object");
    json_builder_set_member_name (builder, "properties");
    json_builder_begin_object (builder);

    /* Property 'a' */
    json_builder_set_member_name (builder, "a");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "number");
    json_builder_set_member_name (builder, "description");
    json_builder_add_string_value (builder, "First operand");
    json_builder_end_object (builder);

    /* Property 'b' */
    json_builder_set_member_name (builder, "b");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "number");
    json_builder_set_member_name (builder, "description");
    json_builder_add_string_value (builder, "Second operand");
    json_builder_end_object (builder);

    json_builder_end_object (builder);

    /* Required fields */
    json_builder_set_member_name (builder, "required");
    json_builder_begin_array (builder);
    json_builder_add_string_value (builder, "a");
    json_builder_add_string_value (builder, "b");
    json_builder_end_array (builder);

    json_builder_end_object (builder);

    return json_builder_get_root (builder);
}

/*
 * create_unary_op_schema:
 * @arg_name: the argument name (e.g., "value")
 * @description: the argument description
 *
 * Creates a JSON schema for a unary operation.
 *
 * Returns: (transfer full): the schema node
 */
static JsonNode *
create_unary_op_schema (const gchar *arg_name,
                        const gchar *description)
{
    g_autoptr(JsonBuilder) builder = json_builder_new ();

    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "object");
    json_builder_set_member_name (builder, "properties");
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, arg_name);
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "number");
    json_builder_set_member_name (builder, "description");
    json_builder_add_string_value (builder, description);
    json_builder_end_object (builder);

    json_builder_end_object (builder);

    json_builder_set_member_name (builder, "required");
    json_builder_begin_array (builder);
    json_builder_add_string_value (builder, arg_name);
    json_builder_end_array (builder);

    json_builder_end_object (builder);

    return json_builder_get_root (builder);
}

/*
 * create_power_schema:
 *
 * Creates a JSON schema for the power operation.
 *
 * Returns: (transfer full): the schema node
 */
static JsonNode *
create_power_schema (void)
{
    g_autoptr(JsonBuilder) builder = json_builder_new ();

    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "object");
    json_builder_set_member_name (builder, "properties");
    json_builder_begin_object (builder);

    /* Property 'base' */
    json_builder_set_member_name (builder, "base");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "number");
    json_builder_set_member_name (builder, "description");
    json_builder_add_string_value (builder, "The base number");
    json_builder_end_object (builder);

    /* Property 'exponent' */
    json_builder_set_member_name (builder, "exponent");
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "type");
    json_builder_add_string_value (builder, "number");
    json_builder_set_member_name (builder, "description");
    json_builder_add_string_value (builder, "The exponent");
    json_builder_end_object (builder);

    json_builder_end_object (builder);

    json_builder_set_member_name (builder, "required");
    json_builder_begin_array (builder);
    json_builder_add_string_value (builder, "base");
    json_builder_add_string_value (builder, "exponent");
    json_builder_end_array (builder);

    json_builder_end_object (builder);

    return json_builder_get_root (builder);
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
add_calculator_tools (McpServer *server)
{
    /* Add tool */
    {
        g_autoptr(McpTool) tool = mcp_tool_new ("add", "Add two numbers");
        g_autoptr(JsonNode) schema = create_binary_op_schema ();
        mcp_tool_set_input_schema (tool, schema);
        mcp_server_add_tool (server, tool, add_handler, NULL, NULL);
    }

    /* Subtract tool */
    {
        g_autoptr(McpTool) tool = mcp_tool_new ("subtract", "Subtract second number from first");
        g_autoptr(JsonNode) schema = create_binary_op_schema ();
        mcp_tool_set_input_schema (tool, schema);
        mcp_server_add_tool (server, tool, subtract_handler, NULL, NULL);
    }

    /* Multiply tool */
    {
        g_autoptr(McpTool) tool = mcp_tool_new ("multiply", "Multiply two numbers");
        g_autoptr(JsonNode) schema = create_binary_op_schema ();
        mcp_tool_set_input_schema (tool, schema);
        mcp_server_add_tool (server, tool, multiply_handler, NULL, NULL);
    }

    /* Divide tool */
    {
        g_autoptr(McpTool) tool = mcp_tool_new ("divide", "Divide first number by second");
        g_autoptr(JsonNode) schema = create_binary_op_schema ();
        mcp_tool_set_input_schema (tool, schema);
        mcp_server_add_tool (server, tool, divide_handler, NULL, NULL);
    }

    /* Square root tool */
    {
        g_autoptr(McpTool) tool = mcp_tool_new ("sqrt", "Compute square root");
        g_autoptr(JsonNode) schema = create_unary_op_schema ("value", "The number to compute square root of");
        mcp_tool_set_input_schema (tool, schema);
        mcp_server_add_tool (server, tool, sqrt_handler, NULL, NULL);
    }

    /* Power tool */
    {
        g_autoptr(McpTool) tool = mcp_tool_new ("power", "Raise base to exponent");
        g_autoptr(JsonNode) schema = create_power_schema ();
        mcp_tool_set_input_schema (tool, schema);
        mcp_server_add_tool (server, tool, power_handler, NULL, NULL);
    }
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

    main_loop = g_main_loop_new (NULL, FALSE);

    /* Create and configure server */
    server = mcp_server_new ("calculator-server", "1.0.0");
    mcp_server_set_instructions (server,
        "This is a calculator server. Available operations:\n"
        "- add(a, b): Returns a + b\n"
        "- subtract(a, b): Returns a - b\n"
        "- multiply(a, b): Returns a * b\n"
        "- divide(a, b): Returns a / b (error if b=0)\n"
        "- sqrt(value): Returns square root (error if negative)\n"
        "- power(base, exponent): Returns base^exponent");

    g_signal_connect (server, "client-disconnected",
                      G_CALLBACK (on_client_disconnected), NULL);

    /* Add all calculator tools */
    add_calculator_tools (server);

    /* Set up transport and start */
    transport = mcp_stdio_transport_new ();
    mcp_server_set_transport (server, MCP_TRANSPORT (transport));

    g_message ("Starting Calculator MCP Server...");
    mcp_server_start_async (server, NULL, NULL, NULL);

    g_main_loop_run (main_loop);

    g_main_loop_unref (main_loop);
    g_message ("Server shut down");

    return 0;
}
