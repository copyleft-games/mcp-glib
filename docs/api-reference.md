# API Reference

Complete API reference for mcp-glib.

## Enumerations

### McpErrorCode

Error codes for MCP operations.

```c
typedef enum {
    MCP_ERROR_PARSE_ERROR        = -32700,  /* JSON parse error */
    MCP_ERROR_INVALID_REQUEST    = -32600,  /* Invalid JSON-RPC request */
    MCP_ERROR_METHOD_NOT_FOUND   = -32601,  /* Method does not exist */
    MCP_ERROR_INVALID_PARAMS     = -32602,  /* Invalid method parameters */
    MCP_ERROR_INTERNAL_ERROR     = -32603,  /* Internal error */
    MCP_ERROR_CONNECTION_CLOSED  = -32000,  /* Connection was closed */
    MCP_ERROR_TRANSPORT_ERROR    = -32001,  /* Transport layer error */
    MCP_ERROR_TIMEOUT            = -32002   /* Request timed out */
} McpErrorCode;
```

### McpTransportState

Transport connection states.

```c
typedef enum {
    MCP_TRANSPORT_STATE_DISCONNECTED,   /* Not connected */
    MCP_TRANSPORT_STATE_CONNECTING,     /* Connection in progress */
    MCP_TRANSPORT_STATE_CONNECTED,      /* Connected */
    MCP_TRANSPORT_STATE_DISCONNECTING,  /* Disconnection in progress */
    MCP_TRANSPORT_STATE_ERROR           /* Error state */
} McpTransportState;
```

### McpRole

Message role in prompts.

```c
typedef enum {
    MCP_ROLE_USER,      /* User message */
    MCP_ROLE_ASSISTANT  /* Assistant message */
} McpRole;
```

### McpLogLevel

Logging levels.

```c
typedef enum {
    MCP_LOG_LEVEL_DEBUG,
    MCP_LOG_LEVEL_INFO,
    MCP_LOG_LEVEL_NOTICE,
    MCP_LOG_LEVEL_WARNING,
    MCP_LOG_LEVEL_ERROR,
    MCP_LOG_LEVEL_CRITICAL,
    MCP_LOG_LEVEL_ALERT,
    MCP_LOG_LEVEL_EMERGENCY
} McpLogLevel;
```

### McpTaskStatus

Task status for long-running operations.

```c
typedef enum {
    MCP_TASK_STATUS_WORKING,        /* Task is running */
    MCP_TASK_STATUS_INPUT_REQUIRED, /* Waiting for input */
    MCP_TASK_STATUS_COMPLETED,      /* Task completed successfully */
    MCP_TASK_STATUS_FAILED,         /* Task failed */
    MCP_TASK_STATUS_CANCELLED       /* Task was cancelled */
} McpTaskStatus;
```

---

## McpServer

MCP server implementation.

### Constructor

```c
McpServer *mcp_server_new (const gchar *name, const gchar *version);
```

Creates a new MCP server.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | gchar* | Server name |
| `version` | gchar* | Server version |
| `instructions` | gchar* | Instructions for AI |

### Methods

```c
/* Configuration */
void mcp_server_set_instructions (McpServer *self, const gchar *instructions);
const gchar *mcp_server_get_instructions (McpServer *self);
void mcp_server_set_transport (McpServer *self, McpTransport *transport);
McpServerCapabilities *mcp_server_get_capabilities (McpServer *self);

/* Tools */
void mcp_server_add_tool (McpServer *self, McpTool *tool,
                          McpToolHandler handler, gpointer user_data,
                          GDestroyNotify destroy);
gboolean mcp_server_remove_tool (McpServer *self, const gchar *name);
void mcp_server_notify_tools_changed (McpServer *self);

/* Resources */
void mcp_server_add_resource (McpServer *self, McpResource *resource,
                              McpResourceHandler handler, gpointer user_data,
                              GDestroyNotify destroy);
void mcp_server_add_resource_template (McpServer *self,
                                       McpResourceTemplate *template,
                                       McpResourceHandler handler,
                                       gpointer user_data, GDestroyNotify destroy);
gboolean mcp_server_remove_resource (McpServer *self, const gchar *uri);
void mcp_server_notify_resources_changed (McpServer *self);
void mcp_server_notify_resource_updated (McpServer *self, const gchar *uri);

/* Prompts */
void mcp_server_add_prompt (McpServer *self, McpPrompt *prompt,
                            McpPromptHandler handler, gpointer user_data,
                            GDestroyNotify destroy);
gboolean mcp_server_remove_prompt (McpServer *self, const gchar *name);
void mcp_server_notify_prompts_changed (McpServer *self);

/* Lifecycle */
void mcp_server_start_async (McpServer *self, GCancellable *cancellable,
                             GAsyncReadyCallback callback, gpointer user_data);
gboolean mcp_server_start_finish (McpServer *self, GAsyncResult *result,
                                  GError **error);

/* Logging */
void mcp_server_emit_log (McpServer *self, McpLogLevel level,
                          const gchar *message, const gchar *logger);
```

### Signals

| Signal | Description |
|--------|-------------|
| `client-initialized` | Client connected and completed handshake |
| `client-disconnected` | Client disconnected |

### Handler Types

```c
/* Tool handler */
typedef McpToolResult *(*McpToolHandler) (McpServer *server,
                                          const gchar *name,
                                          JsonObject *arguments,
                                          gpointer user_data);

/* Resource handler */
typedef GList *(*McpResourceHandler) (McpServer *server,
                                      const gchar *uri,
                                      gpointer user_data);

/* Prompt handler */
typedef McpPromptResult *(*McpPromptHandler) (McpServer *server,
                                              const gchar *name,
                                              GHashTable *arguments,
                                              gpointer user_data);
```

---

## McpClient

MCP client implementation.

### Constructor

```c
McpClient *mcp_client_new (const gchar *name, const gchar *version);
```

Creates a new MCP client.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | gchar* | Client name |
| `version` | gchar* | Client version |

### Methods

```c
/* Configuration */
void mcp_client_set_transport (McpClient *self, McpTransport *transport);
McpClientCapabilities *mcp_client_get_capabilities (McpClient *self);
const gchar *mcp_client_get_server_instructions (McpClient *self);

/* Connection */
void mcp_client_connect_async (McpClient *self, GCancellable *cancellable,
                               GAsyncReadyCallback callback, gpointer user_data);
gboolean mcp_client_connect_finish (McpClient *self, GAsyncResult *result,
                                    GError **error);

/* Tools */
void mcp_client_list_tools_async (McpClient *self, GCancellable *cancellable,
                                  GAsyncReadyCallback callback, gpointer user_data);
GList *mcp_client_list_tools_finish (McpClient *self, GAsyncResult *result,
                                     GError **error);

void mcp_client_call_tool_async (McpClient *self, const gchar *name,
                                 JsonObject *arguments, GCancellable *cancellable,
                                 GAsyncReadyCallback callback, gpointer user_data);
McpToolResult *mcp_client_call_tool_finish (McpClient *self, GAsyncResult *result,
                                            GError **error);

/* Resources */
void mcp_client_list_resources_async (McpClient *self, GCancellable *cancellable,
                                      GAsyncReadyCallback callback, gpointer user_data);
GList *mcp_client_list_resources_finish (McpClient *self, GAsyncResult *result,
                                         GError **error);

void mcp_client_read_resource_async (McpClient *self, const gchar *uri,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback, gpointer user_data);
GList *mcp_client_read_resource_finish (McpClient *self, GAsyncResult *result,
                                        GError **error);

void mcp_client_subscribe_resource (McpClient *self, const gchar *uri);
void mcp_client_unsubscribe_resource (McpClient *self, const gchar *uri);

/* Prompts */
void mcp_client_list_prompts_async (McpClient *self, GCancellable *cancellable,
                                    GAsyncReadyCallback callback, gpointer user_data);
GList *mcp_client_list_prompts_finish (McpClient *self, GAsyncResult *result,
                                       GError **error);

void mcp_client_get_prompt_async (McpClient *self, const gchar *name,
                                  GHashTable *arguments, GCancellable *cancellable,
                                  GAsyncReadyCallback callback, gpointer user_data);
McpPromptResult *mcp_client_get_prompt_finish (McpClient *self, GAsyncResult *result,
                                               GError **error);
```

### Signals

| Signal | Description |
|--------|-------------|
| `tools-changed` | Server tools changed |
| `resources-changed` | Server resources changed |
| `prompts-changed` | Server prompts changed |
| `resource-updated` | Subscribed resource updated |

---

## McpTool

Tool definition.

### Constructor

```c
McpTool *mcp_tool_new (const gchar *name, const gchar *description);
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | gchar* | Tool name |
| `description` | gchar* | Tool description |

### Methods

```c
const gchar *mcp_tool_get_name (McpTool *self);
const gchar *mcp_tool_get_description (McpTool *self);
void mcp_tool_set_input_schema (McpTool *self, JsonNode *schema);
JsonNode *mcp_tool_get_input_schema (McpTool *self);
```

---

## McpResource

Resource definition.

### Constructor

```c
McpResource *mcp_resource_new (const gchar *uri, const gchar *name);
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `uri` | gchar* | Resource URI |
| `name` | gchar* | Resource name |
| `description` | gchar* | Resource description |
| `mime-type` | gchar* | MIME type |

### Methods

```c
const gchar *mcp_resource_get_uri (McpResource *self);
const gchar *mcp_resource_get_name (McpResource *self);
void mcp_resource_set_description (McpResource *self, const gchar *description);
const gchar *mcp_resource_get_description (McpResource *self);
void mcp_resource_set_mime_type (McpResource *self, const gchar *mime_type);
const gchar *mcp_resource_get_mime_type (McpResource *self);
```

---

## McpResourceTemplate

Resource URI template.

### Constructor

```c
McpResourceTemplate *mcp_resource_template_new (const gchar *uri_template,
                                                 const gchar *name);
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `uri-template` | gchar* | URI template (RFC 6570) |
| `name` | gchar* | Template name |
| `description` | gchar* | Template description |
| `mime-type` | gchar* | Default MIME type |

### Methods

```c
const gchar *mcp_resource_template_get_uri_template (McpResourceTemplate *self);
const gchar *mcp_resource_template_get_name (McpResourceTemplate *self);
void mcp_resource_template_set_description (McpResourceTemplate *self, const gchar *desc);
void mcp_resource_template_set_mime_type (McpResourceTemplate *self, const gchar *mime);
```

---

## McpPrompt

Prompt definition.

### Constructor

```c
McpPrompt *mcp_prompt_new (const gchar *name, const gchar *description);
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | gchar* | Prompt name |
| `description` | gchar* | Prompt description |

### Methods

```c
const gchar *mcp_prompt_get_name (McpPrompt *self);
const gchar *mcp_prompt_get_description (McpPrompt *self);
void mcp_prompt_add_argument_full (McpPrompt *self, const gchar *name,
                                   const gchar *description, gboolean required);
GList *mcp_prompt_get_arguments (McpPrompt *self);
```

---

## McpToolResult (Boxed)

Result from a tool invocation.

### Constructor

```c
McpToolResult *mcp_tool_result_new (gboolean is_error);
```

### Methods

```c
void mcp_tool_result_add_text (McpToolResult *self, const gchar *text);
void mcp_tool_result_add_image (McpToolResult *self, const gchar *data,
                                 const gchar *mime_type);
gboolean mcp_tool_result_get_is_error (McpToolResult *self);
JsonArray *mcp_tool_result_get_content (McpToolResult *self);
McpToolResult *mcp_tool_result_ref (McpToolResult *self);
void mcp_tool_result_unref (McpToolResult *self);
```

---

## McpResourceContents (Boxed)

Content from a resource read.

### Constructors

```c
/* Text content */
McpResourceContents *mcp_resource_contents_new_text (const gchar *uri,
                                                      const gchar *text,
                                                      const gchar *mime_type);

/* Binary content (base64) */
McpResourceContents *mcp_resource_contents_new_blob (const gchar *uri,
                                                      const gchar *blob,
                                                      const gchar *mime_type);
```

### Methods

```c
const gchar *mcp_resource_contents_get_uri (McpResourceContents *self);
const gchar *mcp_resource_contents_get_mime_type (McpResourceContents *self);
const gchar *mcp_resource_contents_get_text (McpResourceContents *self);
const gchar *mcp_resource_contents_get_blob (McpResourceContents *self);
McpResourceContents *mcp_resource_contents_ref (McpResourceContents *self);
void mcp_resource_contents_unref (McpResourceContents *self);
```

---

## McpPromptResult (Boxed)

Result from a prompt get.

### Constructor

```c
McpPromptResult *mcp_prompt_result_new (const gchar *description);
```

### Methods

```c
const gchar *mcp_prompt_result_get_description (McpPromptResult *self);
void mcp_prompt_result_add_message (McpPromptResult *self, McpPromptMessage *message);
GList *mcp_prompt_result_get_messages (McpPromptResult *self);
McpPromptResult *mcp_prompt_result_ref (McpPromptResult *self);
void mcp_prompt_result_unref (McpPromptResult *self);
```

---

## McpPromptMessage (Boxed)

Message in a prompt result.

### Constructor

```c
McpPromptMessage *mcp_prompt_message_new (McpRole role);
```

### Methods

```c
McpRole mcp_prompt_message_get_role (McpPromptMessage *self);
void mcp_prompt_message_add_text (McpPromptMessage *self, const gchar *text);
JsonArray *mcp_prompt_message_get_content (McpPromptMessage *self);
McpPromptMessage *mcp_prompt_message_ref (McpPromptMessage *self);
void mcp_prompt_message_unref (McpPromptMessage *self);
```

---

## McpStdioTransport

Stdio-based transport.

### Constructors

```c
/* Use stdin/stdout */
McpStdioTransport *mcp_stdio_transport_new (void);

/* Spawn subprocess */
McpStdioTransport *mcp_stdio_transport_new_subprocess_simple (const gchar *command_line,
                                                               GError **error);
```

---

## McpSession

Base session class (inherited by McpServer and McpClient).

### Methods

```c
McpImplementation *mcp_session_get_remote_implementation (McpSession *self);
```

---

## McpImplementation (Boxed)

Server/client implementation info.

### Methods

```c
const gchar *mcp_implementation_get_name (McpImplementation *self);
const gchar *mcp_implementation_get_version (McpImplementation *self);
```

---

## Error Domain

```c
#define MCP_ERROR (mcp_error_quark ())
GQuark mcp_error_quark (void);
```

Use with `g_error_matches()`:

```c
if (g_error_matches (error, MCP_ERROR, MCP_ERROR_TIMEOUT))
{
    /* Handle timeout */
}
```

---

## McpSamplingMessage (Boxed)

Message for LLM sampling requests.

### Constructor

```c
McpSamplingMessage *mcp_sampling_message_new (McpRole role);
```

### Methods

```c
McpRole mcp_sampling_message_get_role (McpSamplingMessage *self);
void mcp_sampling_message_add_text (McpSamplingMessage *self, const gchar *text);
void mcp_sampling_message_add_image (McpSamplingMessage *self, const gchar *data,
                                      const gchar *mime_type);
GList *mcp_sampling_message_get_content (McpSamplingMessage *self);
JsonNode *mcp_sampling_message_to_json (McpSamplingMessage *self);
McpSamplingMessage *mcp_sampling_message_new_from_json (JsonNode *node, GError **error);
McpSamplingMessage *mcp_sampling_message_ref (McpSamplingMessage *self);
void mcp_sampling_message_unref (McpSamplingMessage *self);
```

---

## McpSamplingResult (Boxed)

Result from LLM sampling.

### Constructor

```c
McpSamplingResult *mcp_sampling_result_new (McpRole role, const gchar *text);
```

### Methods

```c
McpRole mcp_sampling_result_get_role (McpSamplingResult *self);
const gchar *mcp_sampling_result_get_model (McpSamplingResult *self);
void mcp_sampling_result_set_model (McpSamplingResult *self, const gchar *model);
const gchar *mcp_sampling_result_get_stop_reason (McpSamplingResult *self);
void mcp_sampling_result_set_stop_reason (McpSamplingResult *self, const gchar *reason);
JsonNode *mcp_sampling_result_to_json (McpSamplingResult *self);
McpSamplingResult *mcp_sampling_result_new_from_json (JsonNode *node, GError **error);
McpSamplingResult *mcp_sampling_result_ref (McpSamplingResult *self);
void mcp_sampling_result_unref (McpSamplingResult *self);
```

---

## McpModelPreferences (Boxed)

Model preferences for sampling requests.

### Constructor

```c
McpModelPreferences *mcp_model_preferences_new (void);
```

### Methods

```c
void mcp_model_preferences_add_hint (McpModelPreferences *self, const gchar *hint);
GList *mcp_model_preferences_get_hints (McpModelPreferences *self);
void mcp_model_preferences_set_cost_priority (McpModelPreferences *self, gdouble priority);
gdouble mcp_model_preferences_get_cost_priority (McpModelPreferences *self);
void mcp_model_preferences_set_speed_priority (McpModelPreferences *self, gdouble priority);
gdouble mcp_model_preferences_get_speed_priority (McpModelPreferences *self);
void mcp_model_preferences_set_intelligence_priority (McpModelPreferences *self, gdouble priority);
gdouble mcp_model_preferences_get_intelligence_priority (McpModelPreferences *self);
JsonNode *mcp_model_preferences_to_json (McpModelPreferences *self);
McpModelPreferences *mcp_model_preferences_new_from_json (JsonNode *node, GError **error);
McpModelPreferences *mcp_model_preferences_ref (McpModelPreferences *self);
void mcp_model_preferences_unref (McpModelPreferences *self);
```

---

## McpRoot (Boxed)

Filesystem root exposed by client.

### Constructor

```c
McpRoot *mcp_root_new (const gchar *uri, const gchar *name);
```

### Methods

```c
const gchar *mcp_root_get_uri (McpRoot *self);
const gchar *mcp_root_get_name (McpRoot *self);
JsonNode *mcp_root_to_json (McpRoot *self);
McpRoot *mcp_root_new_from_json (JsonNode *node, GError **error);
McpRoot *mcp_root_ref (McpRoot *self);
void mcp_root_unref (McpRoot *self);
```

---

## McpCompletionResult (Boxed)

Result from autocomplete request.

### Constructor

```c
McpCompletionResult *mcp_completion_result_new (void);
```

### Methods

```c
void mcp_completion_result_add_value (McpCompletionResult *self, const gchar *value);
GList *mcp_completion_result_get_values (McpCompletionResult *self);
void mcp_completion_result_set_total (McpCompletionResult *self, gint64 total);
gint64 mcp_completion_result_get_total (McpCompletionResult *self);
void mcp_completion_result_set_has_more (McpCompletionResult *self, gboolean has_more);
gboolean mcp_completion_result_get_has_more (McpCompletionResult *self);
JsonNode *mcp_completion_result_to_json (McpCompletionResult *self);
McpCompletionResult *mcp_completion_result_new_from_json (JsonNode *node, GError **error);
McpCompletionResult *mcp_completion_result_ref (McpCompletionResult *self);
void mcp_completion_result_unref (McpCompletionResult *self);
```

---

## Server Sampling API

Request LLM sampling from connected client.

```c
void mcp_server_request_sampling_async (McpServer *self, GList *messages,
                                        McpModelPreferences *model_preferences,
                                        const gchar *system_prompt, gint64 max_tokens,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback, gpointer user_data);
McpSamplingResult *mcp_server_request_sampling_finish (McpServer *self,
                                                        GAsyncResult *result,
                                                        GError **error);
```

---

## Server Roots API

Request root list from connected client.

```c
void mcp_server_list_roots_async (McpServer *self, GCancellable *cancellable,
                                  GAsyncReadyCallback callback, gpointer user_data);
GList *mcp_server_list_roots_finish (McpServer *self, GAsyncResult *result, GError **error);
```

### Server Signals (Additional)

| Signal | Description |
|--------|-------------|
| `roots-changed` | Client's roots list changed |

---

## Server Completion API

Handle autocomplete requests.

```c
typedef McpCompletionResult *(*McpCompletionHandler) (McpServer *server,
                                                      const gchar *ref_type,
                                                      const gchar *ref_name,
                                                      const gchar *argument_name,
                                                      const gchar *argument_value,
                                                      gpointer user_data);

void mcp_server_set_completion_handler (McpServer *self, McpCompletionHandler handler,
                                        gpointer user_data, GDestroyNotify destroy);
```

---

## Client Sampling API

Handle server sampling requests.

```c
void mcp_client_respond_sampling (McpClient *self, const gchar *request_id,
                                   McpSamplingResult *result);
void mcp_client_reject_sampling (McpClient *self, const gchar *request_id,
                                  gint error_code, const gchar *message);
```

### Client Signals (Additional)

| Signal | Description |
|--------|-------------|
| `sampling-requested` | Server requests LLM sampling |
| `roots-list-requested` | Server requests roots list |

---

## Client Roots API

Manage filesystem roots exposed to server.

```c
void mcp_client_add_root (McpClient *self, McpRoot *root);
void mcp_client_remove_root (McpClient *self, const gchar *uri);
GList *mcp_client_list_roots (McpClient *self);
void mcp_client_notify_roots_changed (McpClient *self);
```

---

## Client Completion API

Request autocomplete from server.

```c
void mcp_client_complete_async (McpClient *self, const gchar *ref_type,
                                const gchar *ref_name, const gchar *argument_name,
                                const gchar *argument_value, GCancellable *cancellable,
                                GAsyncReadyCallback callback, gpointer user_data);
McpCompletionResult *mcp_client_complete_finish (McpClient *self, GAsyncResult *result,
                                                  GError **error);
```
