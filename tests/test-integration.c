/*
 * test-integration.c - End-to-end integration tests for mcp-glib
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Tests server and client communication using a linked mock transport pair.
 */

#define MCP_COMPILATION
#include <mcp/mcp-transport.h>
#include <mcp/mcp-error.h>
#undef MCP_COMPILATION

#include <mcp/mcp.h>

/* ========================================================================== */
/* Linked Mock Transport Pair                                                 */
/*                                                                            */
/* Two transports linked together - messages sent on one are received on the  */
/* other. Used to test server+client communication without real I/O.          */
/* ========================================================================== */

#define TEST_TYPE_LINKED_TRANSPORT (test_linked_transport_get_type ())
G_DECLARE_FINAL_TYPE (TestLinkedTransport, test_linked_transport, TEST, LINKED_TRANSPORT, GObject)

struct _TestLinkedTransport
{
    GObject parent_instance;

    McpTransportState state;

    /* Our peer transport - messages we send go to them */
    TestLinkedTransport *peer;

    /* Pending messages to deliver (from peer) */
    GQueue *receive_queue;

    /* Source for delivering queued messages */
    guint idle_source;
};

static void test_linked_transport_iface_init (McpTransportInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TestLinkedTransport, test_linked_transport, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MCP_TYPE_TRANSPORT,
                                                test_linked_transport_iface_init))

static void
test_linked_transport_finalize (GObject *object)
{
    TestLinkedTransport *self = TEST_LINKED_TRANSPORT (object);

    if (self->idle_source > 0)
        g_source_remove (self->idle_source);

    g_queue_free_full (self->receive_queue, (GDestroyNotify) json_node_unref);

    /* Break the circular reference */
    if (self->peer != NULL)
    {
        self->peer->peer = NULL;
        self->peer = NULL;
    }

    G_OBJECT_CLASS (test_linked_transport_parent_class)->finalize (object);
}

static void
test_linked_transport_class_init (TestLinkedTransportClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = test_linked_transport_finalize;
}

static void
test_linked_transport_init (TestLinkedTransport *self)
{
    self->state = MCP_TRANSPORT_STATE_DISCONNECTED;
    self->receive_queue = g_queue_new ();
}

static McpTransportState
linked_get_state (McpTransport *transport)
{
    TestLinkedTransport *self = TEST_LINKED_TRANSPORT (transport);
    return self->state;
}

static void
linked_connect_async (McpTransport        *transport,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
    TestLinkedTransport *self = TEST_LINKED_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;

    task = g_task_new (self, cancellable, callback, user_data);

    self->state = MCP_TRANSPORT_STATE_CONNECTED;
    mcp_transport_emit_state_changed (transport,
                                      MCP_TRANSPORT_STATE_DISCONNECTED,
                                      MCP_TRANSPORT_STATE_CONNECTED);
    g_task_return_boolean (task, TRUE);
}

static gboolean
linked_connect_finish (McpTransport  *transport,
                       GAsyncResult  *result,
                       GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
linked_disconnect_async (McpTransport        *transport,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
    TestLinkedTransport *self = TEST_LINKED_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;
    McpTransportState old_state;

    task = g_task_new (self, cancellable, callback, user_data);

    old_state = self->state;
    self->state = MCP_TRANSPORT_STATE_DISCONNECTED;
    mcp_transport_emit_state_changed (transport, old_state,
                                      MCP_TRANSPORT_STATE_DISCONNECTED);
    g_task_return_boolean (task, TRUE);
}

static gboolean
linked_disconnect_finish (McpTransport  *transport,
                          GAsyncResult  *result,
                          GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

/* Idle callback to deliver queued messages */
static gboolean
deliver_messages_idle (gpointer user_data)
{
    TestLinkedTransport *self = TEST_LINKED_TRANSPORT (user_data);
    JsonNode *message;

    /* Take a reference to prevent destruction during signal emission */
    g_object_ref (self);

    while ((message = g_queue_pop_head (self->receive_queue)) != NULL)
    {
        mcp_transport_emit_message_received (MCP_TRANSPORT (self), message);
        json_node_unref (message);
    }

    self->idle_source = 0;

    g_object_unref (self);
    return G_SOURCE_REMOVE;
}

static void
linked_send_message_async (McpTransport        *transport,
                           JsonNode            *message,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
    TestLinkedTransport *self = TEST_LINKED_TRANSPORT (transport);
    g_autoptr(GTask) task = NULL;

    task = g_task_new (self, cancellable, callback, user_data);

    if (self->state != MCP_TRANSPORT_STATE_CONNECTED)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_CONNECTION_CLOSED,
                                 "Not connected");
        return;
    }

    if (self->peer == NULL)
    {
        g_task_return_new_error (task, MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                                 "No peer transport");
        return;
    }

    /* Queue message for delivery to peer */
    g_queue_push_tail (self->peer->receive_queue, json_node_copy (message));

    /* Schedule delivery in an idle callback */
    if (self->peer->idle_source == 0)
    {
        self->peer->idle_source = g_idle_add (deliver_messages_idle, self->peer);
    }

    g_task_return_boolean (task, TRUE);
}

static gboolean
linked_send_message_finish (McpTransport  *transport,
                            GAsyncResult  *result,
                            GError       **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
test_linked_transport_iface_init (McpTransportInterface *iface)
{
    iface->get_state = linked_get_state;
    iface->connect_async = linked_connect_async;
    iface->connect_finish = linked_connect_finish;
    iface->disconnect_async = linked_disconnect_async;
    iface->disconnect_finish = linked_disconnect_finish;
    iface->send_message_async = linked_send_message_async;
    iface->send_message_finish = linked_send_message_finish;
}

/* Create a linked pair of transports */
static void
test_linked_transport_create_pair (TestLinkedTransport **server_transport,
                                   TestLinkedTransport **client_transport)
{
    TestLinkedTransport *st, *ct;

    st = g_object_new (TEST_TYPE_LINKED_TRANSPORT, NULL);
    ct = g_object_new (TEST_TYPE_LINKED_TRANSPORT, NULL);

    /* Link them together */
    st->peer = ct;
    ct->peer = st;

    *server_transport = st;
    *client_transport = ct;
}

/* ========================================================================== */
/* Test Fixtures                                                              */
/* ========================================================================== */

typedef struct
{
    McpServer           *server;
    McpClient           *client;
    TestLinkedTransport *server_transport;
    TestLinkedTransport *client_transport;
    GMainLoop           *loop;

    /* Async operation tracking */
    gboolean callback_called;
    gboolean success;
    GError  *error;

    /* Generic result storage */
    gpointer result_ptr;
    GList   *result_list;
} IntegrationFixture;

static void
integration_fixture_setup (IntegrationFixture *fixture,
                           gconstpointer       user_data)
{
    fixture->server = mcp_server_new ("test-server", "1.0.0");
    fixture->client = mcp_client_new ("test-client", "1.0.0");

    test_linked_transport_create_pair (&fixture->server_transport,
                                       &fixture->client_transport);

    mcp_server_set_transport (fixture->server,
                              MCP_TRANSPORT (fixture->server_transport));
    mcp_client_set_transport (fixture->client,
                              MCP_TRANSPORT (fixture->client_transport));

    fixture->loop = g_main_loop_new (NULL, FALSE);
    fixture->callback_called = FALSE;
    fixture->success = FALSE;
    fixture->error = NULL;
    fixture->result_ptr = NULL;
    fixture->result_list = NULL;
}

static void
integration_fixture_teardown (IntegrationFixture *fixture,
                              gconstpointer       user_data)
{
    if (fixture->result_list != NULL)
        g_list_free_full (fixture->result_list, g_object_unref);

    g_clear_object (&fixture->server);
    g_clear_object (&fixture->client);
    g_clear_object (&fixture->server_transport);
    g_clear_object (&fixture->client_transport);
    g_clear_pointer (&fixture->loop, g_main_loop_unref);
    g_clear_error (&fixture->error);
}

/* Helper to run main loop for a short time */
static void
run_loop_briefly (IntegrationFixture *fixture)
{
    GMainContext *ctx = g_main_loop_get_context (fixture->loop);
    gint iterations = 0;

    /* Process pending events for up to 100 iterations */
    while (g_main_context_pending (ctx) && iterations < 100)
    {
        g_main_context_iteration (ctx, FALSE);
        iterations++;
    }
}

/* Timeout callback that quits the main loop */
static gboolean
timeout_quit_loop (gpointer user_data)
{
    GMainLoop *loop = user_data;
    g_main_loop_quit (loop);
    return G_SOURCE_REMOVE;
}

/* ========================================================================== */
/* Callbacks                                                                  */
/* ========================================================================== */

static void
client_connect_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    IntegrationFixture *fixture = user_data;
    McpClient *client = MCP_CLIENT (source);

    fixture->callback_called = TRUE;
    fixture->success = mcp_client_connect_finish (client, result, &fixture->error);
    g_main_loop_quit (fixture->loop);
}

static void
list_tools_cb (GObject      *source,
               GAsyncResult *result,
               gpointer      user_data)
{
    IntegrationFixture *fixture = user_data;
    McpClient *client = MCP_CLIENT (source);

    fixture->callback_called = TRUE;
    fixture->result_list = mcp_client_list_tools_finish (client, result, &fixture->error);
    fixture->success = (fixture->error == NULL);
    g_main_loop_quit (fixture->loop);
}

static void
call_tool_cb (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
    IntegrationFixture *fixture = user_data;
    McpClient *client = MCP_CLIENT (source);

    fixture->callback_called = TRUE;
    fixture->result_ptr = mcp_client_call_tool_finish (client, result, &fixture->error);
    fixture->success = (fixture->error == NULL);
    g_main_loop_quit (fixture->loop);
}

static void
list_resources_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    IntegrationFixture *fixture = user_data;
    McpClient *client = MCP_CLIENT (source);

    fixture->callback_called = TRUE;
    fixture->result_list = mcp_client_list_resources_finish (client, result, &fixture->error);
    fixture->success = (fixture->error == NULL);
    g_main_loop_quit (fixture->loop);
}

static void
read_resource_cb (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    IntegrationFixture *fixture = user_data;
    McpClient *client = MCP_CLIENT (source);

    fixture->callback_called = TRUE;
    fixture->result_list = mcp_client_read_resource_finish (client, result, &fixture->error);
    fixture->success = (fixture->error == NULL);
    g_main_loop_quit (fixture->loop);
}

static void
list_prompts_cb (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    IntegrationFixture *fixture = user_data;
    McpClient *client = MCP_CLIENT (source);

    fixture->callback_called = TRUE;
    fixture->result_list = mcp_client_list_prompts_finish (client, result, &fixture->error);
    fixture->success = (fixture->error == NULL);
    g_main_loop_quit (fixture->loop);
}

static void
get_prompt_cb (GObject      *source,
               GAsyncResult *result,
               gpointer      user_data)
{
    IntegrationFixture *fixture = user_data;
    McpClient *client = MCP_CLIENT (source);

    fixture->callback_called = TRUE;
    fixture->result_ptr = mcp_client_get_prompt_finish (client, result, &fixture->error);
    fixture->success = (fixture->error == NULL);
    g_main_loop_quit (fixture->loop);
}

static void
ping_cb (GObject      *source,
         GAsyncResult *result,
         gpointer      user_data)
{
    IntegrationFixture *fixture = user_data;
    McpClient *client = MCP_CLIENT (source);

    fixture->callback_called = TRUE;
    fixture->success = mcp_client_ping_finish (client, result, &fixture->error);
    g_main_loop_quit (fixture->loop);
}

/* ========================================================================== */
/* Tool Handler                                                               */
/* ========================================================================== */

static McpToolResult *
test_add_handler (McpServer   *server,
                  const gchar *name,
                  JsonObject  *arguments,
                  gpointer     user_data)
{
    McpToolResult *result;
    gint64 a, b, sum;
    g_autofree gchar *text = NULL;

    a = json_object_get_int_member_with_default (arguments, "a", 0);
    b = json_object_get_int_member_with_default (arguments, "b", 0);
    sum = a + b;

    text = g_strdup_printf ("%" G_GINT64_FORMAT, sum);

    result = mcp_tool_result_new (FALSE);
    mcp_tool_result_add_text (result, text);

    return result;
}

/* ========================================================================== */
/* Resource Handler                                                           */
/* ========================================================================== */

static GList *
test_resource_handler (McpServer   *server,
                       const gchar *uri,
                       gpointer     user_data)
{
    g_autoptr(McpResourceContents) contents = NULL;
    GList *list = NULL;

    contents = mcp_resource_contents_new_text (uri,
                                                "Hello from test resource!",
                                                "text/plain");
    list = g_list_append (NULL, g_steal_pointer (&contents));

    return list;
}

/* ========================================================================== */
/* Prompt Handler                                                             */
/* ========================================================================== */

static McpPromptResult *
test_prompt_handler (McpServer   *server,
                     const gchar *name,
                     GHashTable  *arguments,
                     gpointer     user_data)
{
    McpPromptResult *result;
    McpPromptMessage *msg;
    g_autofree gchar *text = NULL;
    const gchar *arg_value;

    arg_value = arguments ? g_hash_table_lookup (arguments, "subject") : NULL;
    if (arg_value == NULL)
        arg_value = "world";

    text = g_strdup_printf ("Hello, %s!", arg_value);

    msg = mcp_prompt_message_new (MCP_ROLE_ASSISTANT);
    mcp_prompt_message_add_text (msg, text);

    result = mcp_prompt_result_new (NULL);
    mcp_prompt_result_add_message (result, msg);
    mcp_prompt_message_unref (msg);

    return result;
}

/* ========================================================================== */
/* Helper to connect client and start server                                  */
/* ========================================================================== */

static gboolean
connect_client_and_server (IntegrationFixture *fixture)
{
    guint timeout_id;
    gboolean timed_out = FALSE;

    /* Start server (async - will wait for client) */
    mcp_server_start_async (fixture->server, NULL, NULL, NULL);

    /* Let server start processing */
    run_loop_briefly (fixture);

    /* Connect client */
    mcp_client_connect_async (fixture->client, NULL, client_connect_cb, fixture);

    /* Set a timeout to prevent infinite loop */
    timeout_id = g_timeout_add (5000, timeout_quit_loop, fixture->loop);

    g_main_loop_run (fixture->loop);

    /* Remove timeout if we finished normally */
    if (g_source_remove (timeout_id) == FALSE)
        timed_out = TRUE;

    if (timed_out)
    {
        g_printerr ("Timeout waiting for client connection\n");
        return FALSE;
    }

    return fixture->success && fixture->error == NULL;
}

/* ========================================================================== */
/* Tests                                                                      */
/* ========================================================================== */

/* Test basic initialization handshake */
static void
test_initialization (IntegrationFixture *fixture,
                     gconstpointer       user_data)
{
    gboolean connected;

    connected = connect_client_and_server (fixture);

    g_assert_true (connected);
    g_assert_no_error (fixture->error);

    /* Check that server knows about client capabilities */
    g_assert_nonnull (mcp_server_get_client_capabilities (fixture->server));

    /* Check that client knows about server capabilities */
    g_assert_nonnull (mcp_client_get_server_capabilities (fixture->client));
}

/* Test ping */
static void
test_ping (IntegrationFixture *fixture,
           gconstpointer       user_data)
{
    gboolean connected;

    connected = connect_client_and_server (fixture);
    g_assert_true (connected);

    /* Reset fixture state */
    fixture->callback_called = FALSE;
    fixture->success = FALSE;
    g_clear_error (&fixture->error);

    /* Send ping */
    mcp_client_ping_async (fixture->client, NULL, ping_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);
}

/* Test tool listing */
static void
test_list_tools (IntegrationFixture *fixture,
                 gconstpointer       user_data)
{
    g_autoptr(McpTool) tool = NULL;
    gboolean connected;
    GList *tools;
    McpTool *found;

    /* Add a test tool */
    tool = mcp_tool_new ("add", "Adds two numbers");
    mcp_server_add_tool (fixture->server, tool, test_add_handler, NULL, NULL);

    connected = connect_client_and_server (fixture);
    g_assert_true (connected);

    /* Reset fixture state */
    fixture->callback_called = FALSE;
    fixture->success = FALSE;
    g_clear_error (&fixture->error);

    /* List tools */
    mcp_client_list_tools_async (fixture->client, NULL, list_tools_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);

    tools = fixture->result_list;
    g_assert_nonnull (tools);
    g_assert_cmpuint (g_list_length (tools), ==, 1);

    found = tools->data;
    g_assert_cmpstr (mcp_tool_get_name (found), ==, "add");
    g_assert_cmpstr (mcp_tool_get_description (found), ==, "Adds two numbers");
}

/* Test tool calling */
static void
test_call_tool (IntegrationFixture *fixture,
                gconstpointer       user_data)
{
    g_autoptr(McpTool) tool = NULL;
    g_autoptr(JsonObject) args = NULL;
    gboolean connected;
    McpToolResult *result;
    JsonArray *content_array;
    JsonObject *content_obj;
    const gchar *text;

    /* Add a test tool */
    tool = mcp_tool_new ("add", "Adds two numbers");
    mcp_server_add_tool (fixture->server, tool, test_add_handler, NULL, NULL);

    connected = connect_client_and_server (fixture);
    g_assert_true (connected);

    /* Reset fixture state */
    fixture->callback_called = FALSE;
    fixture->success = FALSE;
    g_clear_error (&fixture->error);

    /* Build arguments */
    args = json_object_new ();
    json_object_set_int_member (args, "a", 5);
    json_object_set_int_member (args, "b", 3);

    /* Call the tool */
    mcp_client_call_tool_async (fixture->client, "add", args, NULL, call_tool_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);

    result = fixture->result_ptr;
    g_assert_nonnull (result);
    g_assert_false (mcp_tool_result_get_is_error (result));

    content_array = mcp_tool_result_get_content (result);
    g_assert_nonnull (content_array);
    g_assert_cmpuint (json_array_get_length (content_array), ==, 1);

    content_obj = json_array_get_object_element (content_array, 0);
    g_assert_cmpstr (json_object_get_string_member (content_obj, "type"), ==, "text");
    text = json_object_get_string_member (content_obj, "text");
    g_assert_cmpstr (text, ==, "8");

    mcp_tool_result_unref (result);
}

/* Test resource listing */
static void
test_list_resources (IntegrationFixture *fixture,
                     gconstpointer       user_data)
{
    g_autoptr(McpResource) resource = NULL;
    gboolean connected;
    GList *resources;
    McpResource *found;

    /* Add a test resource */
    resource = mcp_resource_new ("test://data", "Test Data");
    mcp_resource_set_description (resource, "A test resource");
    mcp_resource_set_mime_type (resource, "text/plain");
    mcp_server_add_resource (fixture->server, resource, test_resource_handler, NULL, NULL);

    connected = connect_client_and_server (fixture);
    g_assert_true (connected);

    /* Reset fixture state */
    fixture->callback_called = FALSE;
    fixture->success = FALSE;
    g_clear_error (&fixture->error);

    /* List resources */
    mcp_client_list_resources_async (fixture->client, NULL, list_resources_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);

    resources = fixture->result_list;
    g_assert_nonnull (resources);
    g_assert_cmpuint (g_list_length (resources), ==, 1);

    found = resources->data;
    g_assert_cmpstr (mcp_resource_get_uri (found), ==, "test://data");
    g_assert_cmpstr (mcp_resource_get_name (found), ==, "Test Data");
}

/* Test resource reading */
static void
test_read_resource (IntegrationFixture *fixture,
                    gconstpointer       user_data)
{
    g_autoptr(McpResource) resource = NULL;
    gboolean connected;
    GList *contents;
    McpResourceContents *found;

    /* Add a test resource */
    resource = mcp_resource_new ("test://hello", "Hello Resource");
    mcp_server_add_resource (fixture->server, resource, test_resource_handler, NULL, NULL);

    connected = connect_client_and_server (fixture);
    g_assert_true (connected);

    /* Reset fixture state */
    fixture->callback_called = FALSE;
    fixture->success = FALSE;
    g_clear_error (&fixture->error);

    /* Read resource */
    mcp_client_read_resource_async (fixture->client, "test://hello", NULL,
                                     read_resource_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);

    contents = fixture->result_list;
    g_assert_nonnull (contents);
    g_assert_cmpuint (g_list_length (contents), ==, 1);

    found = contents->data;
    g_assert_cmpstr (mcp_resource_contents_get_uri (found), ==, "test://hello");
    g_assert_cmpstr (mcp_resource_contents_get_text (found), ==, "Hello from test resource!");

    /* Clean up McpResourceContents (boxed type, not GObject) */
    g_list_free_full (fixture->result_list, (GDestroyNotify) mcp_resource_contents_unref);
    fixture->result_list = NULL;
}

/* Test prompt listing */
static void
test_list_prompts (IntegrationFixture *fixture,
                   gconstpointer       user_data)
{
    g_autoptr(McpPrompt) prompt = NULL;
    gboolean connected;
    GList *prompts;
    McpPrompt *found;

    /* Add a test prompt */
    prompt = mcp_prompt_new ("greeting", "A greeting prompt");
    mcp_server_add_prompt (fixture->server, prompt, test_prompt_handler, NULL, NULL);

    connected = connect_client_and_server (fixture);
    g_assert_true (connected);

    /* Reset fixture state */
    fixture->callback_called = FALSE;
    fixture->success = FALSE;
    g_clear_error (&fixture->error);

    /* List prompts */
    mcp_client_list_prompts_async (fixture->client, NULL, list_prompts_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);

    prompts = fixture->result_list;
    g_assert_nonnull (prompts);
    g_assert_cmpuint (g_list_length (prompts), ==, 1);

    found = prompts->data;
    g_assert_cmpstr (mcp_prompt_get_name (found), ==, "greeting");
    g_assert_cmpstr (mcp_prompt_get_description (found), ==, "A greeting prompt");
}

/* Test prompt getting */
static void
test_get_prompt (IntegrationFixture *fixture,
                 gconstpointer       user_data)
{
    g_autoptr(McpPrompt) prompt = NULL;
    g_autoptr(GHashTable) args = NULL;
    gboolean connected;
    McpPromptResult *result;
    GList *messages;
    McpPromptMessage *msg;
    JsonArray *content_array;
    JsonObject *content_obj;
    const gchar *text;

    /* Add a test prompt */
    prompt = mcp_prompt_new ("greeting", "A greeting prompt");
    mcp_server_add_prompt (fixture->server, prompt, test_prompt_handler, NULL, NULL);

    connected = connect_client_and_server (fixture);
    g_assert_true (connected);

    /* Reset fixture state */
    fixture->callback_called = FALSE;
    fixture->success = FALSE;
    g_clear_error (&fixture->error);

    /* Build arguments */
    args = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert (args, g_strdup ("subject"), g_strdup ("Claude"));

    /* Get prompt */
    mcp_client_get_prompt_async (fixture->client, "greeting", args, NULL,
                                  get_prompt_cb, fixture);
    g_main_loop_run (fixture->loop);

    g_assert_true (fixture->callback_called);
    g_assert_true (fixture->success);
    g_assert_no_error (fixture->error);

    result = fixture->result_ptr;
    g_assert_nonnull (result);

    messages = mcp_prompt_result_get_messages (result);
    g_assert_nonnull (messages);
    g_assert_cmpuint (g_list_length (messages), ==, 1);

    msg = messages->data;
    g_assert_cmpint (mcp_prompt_message_get_role (msg), ==, MCP_ROLE_ASSISTANT);

    content_array = mcp_prompt_message_get_content (msg);
    g_assert_nonnull (content_array);
    g_assert_cmpuint (json_array_get_length (content_array), >=, 1);

    content_obj = json_array_get_object_element (content_array, 0);
    g_assert_cmpstr (json_object_get_string_member (content_obj, "type"), ==, "text");
    text = json_object_get_string_member (content_obj, "text");
    g_assert_cmpstr (text, ==, "Hello, Claude!");

    mcp_prompt_result_unref (result);
}

/* Test server instructions */
static void
test_server_instructions (IntegrationFixture *fixture,
                          gconstpointer       user_data)
{
    gboolean connected;
    const gchar *instructions;

    /* Set server instructions */
    mcp_server_set_instructions (fixture->server, "You are a helpful assistant.");

    connected = connect_client_and_server (fixture);
    g_assert_true (connected);

    /* Client should receive the instructions */
    instructions = mcp_client_get_server_instructions (fixture->client);
    g_assert_cmpstr (instructions, ==, "You are a helpful assistant.");
}

/* ========================================================================== */
/* Main                                                                       */
/* ========================================================================== */

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add ("/integration/initialization",
                IntegrationFixture, NULL,
                integration_fixture_setup,
                test_initialization,
                integration_fixture_teardown);

    g_test_add ("/integration/ping",
                IntegrationFixture, NULL,
                integration_fixture_setup,
                test_ping,
                integration_fixture_teardown);

    g_test_add ("/integration/list-tools",
                IntegrationFixture, NULL,
                integration_fixture_setup,
                test_list_tools,
                integration_fixture_teardown);

    g_test_add ("/integration/call-tool",
                IntegrationFixture, NULL,
                integration_fixture_setup,
                test_call_tool,
                integration_fixture_teardown);

    g_test_add ("/integration/list-resources",
                IntegrationFixture, NULL,
                integration_fixture_setup,
                test_list_resources,
                integration_fixture_teardown);

    g_test_add ("/integration/read-resource",
                IntegrationFixture, NULL,
                integration_fixture_setup,
                test_read_resource,
                integration_fixture_teardown);

    g_test_add ("/integration/list-prompts",
                IntegrationFixture, NULL,
                integration_fixture_setup,
                test_list_prompts,
                integration_fixture_teardown);

    g_test_add ("/integration/get-prompt",
                IntegrationFixture, NULL,
                integration_fixture_setup,
                test_get_prompt,
                integration_fixture_teardown);

    g_test_add ("/integration/server-instructions",
                IntegrationFixture, NULL,
                integration_fixture_setup,
                test_server_instructions,
                integration_fixture_teardown);

    return g_test_run ();
}
