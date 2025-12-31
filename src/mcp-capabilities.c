/*
 * mcp-capabilities.c - Capability negotiation types implementation
 *
 * Copyright (C) 2025 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "mcp-capabilities.h"
#include "mcp-error.h"
#undef MCP_COMPILATION

/* ========================================================================== */
/* McpImplementation                                                          */
/* ========================================================================== */

struct _McpImplementation
{
    GObject parent_instance;

    gchar *name;
    gchar *version;
    gchar *title;
    gchar *website_url;
};

G_DEFINE_TYPE (McpImplementation, mcp_implementation, G_TYPE_OBJECT)

enum
{
    PROP_IMPL_0,
    PROP_IMPL_NAME,
    PROP_IMPL_VERSION,
    PROP_IMPL_TITLE,
    PROP_IMPL_WEBSITE_URL,
    N_IMPL_PROPERTIES
};

static GParamSpec *impl_properties[N_IMPL_PROPERTIES];

static void
mcp_implementation_finalize (GObject *object)
{
    McpImplementation *self = MCP_IMPLEMENTATION (object);

    g_clear_pointer (&self->name, g_free);
    g_clear_pointer (&self->version, g_free);
    g_clear_pointer (&self->title, g_free);
    g_clear_pointer (&self->website_url, g_free);

    G_OBJECT_CLASS (mcp_implementation_parent_class)->finalize (object);
}

static void
mcp_implementation_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    McpImplementation *self = MCP_IMPLEMENTATION (object);

    switch (prop_id)
    {
        case PROP_IMPL_NAME:
            g_value_set_string (value, self->name);
            break;
        case PROP_IMPL_VERSION:
            g_value_set_string (value, self->version);
            break;
        case PROP_IMPL_TITLE:
            g_value_set_string (value, self->title);
            break;
        case PROP_IMPL_WEBSITE_URL:
            g_value_set_string (value, self->website_url);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_implementation_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    McpImplementation *self = MCP_IMPLEMENTATION (object);

    switch (prop_id)
    {
        case PROP_IMPL_NAME:
            g_free (self->name);
            self->name = g_value_dup_string (value);
            break;
        case PROP_IMPL_VERSION:
            g_free (self->version);
            self->version = g_value_dup_string (value);
            break;
        case PROP_IMPL_TITLE:
            g_free (self->title);
            self->title = g_value_dup_string (value);
            break;
        case PROP_IMPL_WEBSITE_URL:
            g_free (self->website_url);
            self->website_url = g_value_dup_string (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
mcp_implementation_class_init (McpImplementationClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = mcp_implementation_finalize;
    object_class->get_property = mcp_implementation_get_property;
    object_class->set_property = mcp_implementation_set_property;

    impl_properties[PROP_IMPL_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "Implementation name",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT |
                             G_PARAM_STATIC_STRINGS);

    impl_properties[PROP_IMPL_VERSION] =
        g_param_spec_string ("version",
                             "Version",
                             "Implementation version",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT |
                             G_PARAM_STATIC_STRINGS);

    impl_properties[PROP_IMPL_TITLE] =
        g_param_spec_string ("title",
                             "Title",
                             "Human-readable title",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    impl_properties[PROP_IMPL_WEBSITE_URL] =
        g_param_spec_string ("website-url",
                             "Website URL",
                             "Website URL",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_IMPL_PROPERTIES, impl_properties);
}

static void
mcp_implementation_init (McpImplementation *self)
{
    (void)self;
}

/**
 * mcp_implementation_new:
 * @name: the implementation name
 * @version: the implementation version
 *
 * Creates a new implementation info object.
 *
 * Returns: (transfer full): a new #McpImplementation
 */
McpImplementation *
mcp_implementation_new (const gchar *name,
                        const gchar *version)
{
    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (version != NULL, NULL);

    return g_object_new (MCP_TYPE_IMPLEMENTATION,
                         "name", name,
                         "version", version,
                         NULL);
}

const gchar *
mcp_implementation_get_name (McpImplementation *self)
{
    g_return_val_if_fail (MCP_IS_IMPLEMENTATION (self), NULL);
    return self->name;
}

const gchar *
mcp_implementation_get_version (McpImplementation *self)
{
    g_return_val_if_fail (MCP_IS_IMPLEMENTATION (self), NULL);
    return self->version;
}

const gchar *
mcp_implementation_get_title (McpImplementation *self)
{
    g_return_val_if_fail (MCP_IS_IMPLEMENTATION (self), NULL);
    return self->title;
}

void
mcp_implementation_set_title (McpImplementation *self,
                              const gchar       *title)
{
    g_return_if_fail (MCP_IS_IMPLEMENTATION (self));
    g_free (self->title);
    self->title = g_strdup (title);
}

const gchar *
mcp_implementation_get_website_url (McpImplementation *self)
{
    g_return_val_if_fail (MCP_IS_IMPLEMENTATION (self), NULL);
    return self->website_url;
}

void
mcp_implementation_set_website_url (McpImplementation *self,
                                    const gchar       *url)
{
    g_return_if_fail (MCP_IS_IMPLEMENTATION (self));
    g_free (self->website_url);
    self->website_url = g_strdup (url);
}

/**
 * mcp_implementation_to_json:
 * @self: an #McpImplementation
 *
 * Serializes to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *
mcp_implementation_to_json (McpImplementation *self)
{
    g_autoptr(JsonBuilder) builder = NULL;

    g_return_val_if_fail (MCP_IS_IMPLEMENTATION (self), NULL);

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "name");
    json_builder_add_string_value (builder, self->name);

    json_builder_set_member_name (builder, "version");
    json_builder_add_string_value (builder, self->version);

    if (self->title != NULL)
    {
        json_builder_set_member_name (builder, "title");
        json_builder_add_string_value (builder, self->title);
    }

    if (self->website_url != NULL)
    {
        json_builder_set_member_name (builder, "websiteUrl");
        json_builder_add_string_value (builder, self->website_url);
    }

    json_builder_end_object (builder);

    return json_builder_get_root (builder);
}

/**
 * mcp_implementation_new_from_json:
 * @node: a #JsonNode
 * @error: (nullable): return location for a #GError
 *
 * Creates from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpImplementation, or %NULL
 */
McpImplementation *
mcp_implementation_new_from_json (JsonNode  *node,
                                  GError   **error)
{
    JsonObject *obj;
    const gchar *name;
    const gchar *version;
    McpImplementation *impl;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Implementation must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);

    if (!json_object_has_member (obj, "name"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Implementation missing 'name' field");
        return NULL;
    }

    if (!json_object_has_member (obj, "version"))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "Implementation missing 'version' field");
        return NULL;
    }

    name = json_object_get_string_member (obj, "name");
    version = json_object_get_string_member (obj, "version");

    impl = mcp_implementation_new (name, version);

    if (json_object_has_member (obj, "title"))
    {
        impl->title = g_strdup (json_object_get_string_member (obj, "title"));
    }

    if (json_object_has_member (obj, "websiteUrl"))
    {
        impl->website_url = g_strdup (json_object_get_string_member (obj, "websiteUrl"));
    }

    return impl;
}
/* ========================================================================== */
/* McpServerCapabilities                                                      */
/* ========================================================================== */

struct _McpServerCapabilities
{
    GObject parent_instance;

    /* Logging - marker capability */
    gboolean logging;

    /* Prompts capability */
    gboolean prompts;
    gboolean prompts_list_changed;

    /* Resources capability */
    gboolean resources;
    gboolean resources_subscribe;
    gboolean resources_list_changed;

    /* Tools capability */
    gboolean tools;
    gboolean tools_list_changed;

    /* Completions - marker capability */
    gboolean completions;

    /* Tasks - marker capability (experimental) */
    gboolean tasks;

    /* Experimental capabilities */
    GHashTable *experimental;
};

G_DEFINE_TYPE (McpServerCapabilities, mcp_server_capabilities, G_TYPE_OBJECT)

static void
mcp_server_capabilities_finalize (GObject *object)
{
    McpServerCapabilities *self = MCP_SERVER_CAPABILITIES (object);

    g_clear_pointer (&self->experimental, g_hash_table_unref);

    G_OBJECT_CLASS (mcp_server_capabilities_parent_class)->finalize (object);
}

static void
mcp_server_capabilities_class_init (McpServerCapabilitiesClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = mcp_server_capabilities_finalize;
}

static void
mcp_server_capabilities_init (McpServerCapabilities *self)
{
    self->experimental = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free,
                                                 (GDestroyNotify)json_node_unref);
}

/**
 * mcp_server_capabilities_new:
 *
 * Creates a new server capabilities object.
 *
 * Returns: (transfer full): a new #McpServerCapabilities
 */
McpServerCapabilities *
mcp_server_capabilities_new (void)
{
    return g_object_new (MCP_TYPE_SERVER_CAPABILITIES, NULL);
}

/* Logging */

gboolean
mcp_server_capabilities_get_logging (McpServerCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), FALSE);
    return self->logging;
}

void
mcp_server_capabilities_set_logging (McpServerCapabilities *self,
                                     gboolean               enabled)
{
    g_return_if_fail (MCP_IS_SERVER_CAPABILITIES (self));
    self->logging = enabled;
}

/* Prompts */

gboolean
mcp_server_capabilities_get_prompts (McpServerCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), FALSE);
    return self->prompts;
}

void
mcp_server_capabilities_set_prompts (McpServerCapabilities *self,
                                     gboolean               enabled,
                                     gboolean               list_changed)
{
    g_return_if_fail (MCP_IS_SERVER_CAPABILITIES (self));
    self->prompts = enabled;
    self->prompts_list_changed = list_changed;
}

gboolean
mcp_server_capabilities_get_prompts_list_changed (McpServerCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), FALSE);
    return self->prompts_list_changed;
}

/* Resources */

gboolean
mcp_server_capabilities_get_resources (McpServerCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), FALSE);
    return self->resources;
}

void
mcp_server_capabilities_set_resources (McpServerCapabilities *self,
                                       gboolean               enabled,
                                       gboolean               subscribe,
                                       gboolean               list_changed)
{
    g_return_if_fail (MCP_IS_SERVER_CAPABILITIES (self));
    self->resources = enabled;
    self->resources_subscribe = subscribe;
    self->resources_list_changed = list_changed;
}

gboolean
mcp_server_capabilities_get_resources_subscribe (McpServerCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), FALSE);
    return self->resources_subscribe;
}

gboolean
mcp_server_capabilities_get_resources_list_changed (McpServerCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), FALSE);
    return self->resources_list_changed;
}

/* Tools */

gboolean
mcp_server_capabilities_get_tools (McpServerCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), FALSE);
    return self->tools;
}

void
mcp_server_capabilities_set_tools (McpServerCapabilities *self,
                                   gboolean               enabled,
                                   gboolean               list_changed)
{
    g_return_if_fail (MCP_IS_SERVER_CAPABILITIES (self));
    self->tools = enabled;
    self->tools_list_changed = list_changed;
}

gboolean
mcp_server_capabilities_get_tools_list_changed (McpServerCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), FALSE);
    return self->tools_list_changed;
}

/* Completions */

gboolean
mcp_server_capabilities_get_completions (McpServerCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), FALSE);
    return self->completions;
}

void
mcp_server_capabilities_set_completions (McpServerCapabilities *self,
                                         gboolean               enabled)
{
    g_return_if_fail (MCP_IS_SERVER_CAPABILITIES (self));
    self->completions = enabled;
}

/* Tasks */

gboolean
mcp_server_capabilities_get_tasks (McpServerCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), FALSE);
    return self->tasks;
}

void
mcp_server_capabilities_set_tasks (McpServerCapabilities *self,
                                   gboolean               enabled)
{
    g_return_if_fail (MCP_IS_SERVER_CAPABILITIES (self));
    self->tasks = enabled;
}

/* Experimental */

void
mcp_server_capabilities_set_experimental (McpServerCapabilities *self,
                                          const gchar           *name,
                                          JsonNode              *config)
{
    g_return_if_fail (MCP_IS_SERVER_CAPABILITIES (self));
    g_return_if_fail (name != NULL);

    if (config != NULL)
    {
        g_hash_table_insert (self->experimental,
                             g_strdup (name),
                             json_node_copy (config));
    }
    else
    {
        /* Store empty object to indicate capability presence */
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        json_builder_begin_object (builder);
        json_builder_end_object (builder);
        g_hash_table_insert (self->experimental,
                             g_strdup (name),
                             json_builder_get_root (builder));
    }
}

JsonNode *
mcp_server_capabilities_get_experimental (McpServerCapabilities *self,
                                          const gchar           *name)
{
    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (self->experimental, name);
}

/**
 * mcp_server_capabilities_to_json:
 * @self: an #McpServerCapabilities
 *
 * Serializes to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *
mcp_server_capabilities_to_json (McpServerCapabilities *self)
{
    g_autoptr(JsonBuilder) builder = NULL;
    GHashTableIter iter;
    gpointer key, value;

    g_return_val_if_fail (MCP_IS_SERVER_CAPABILITIES (self), NULL);

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    /* Logging - marker object */
    if (self->logging)
    {
        json_builder_set_member_name (builder, "logging");
        json_builder_begin_object (builder);
        json_builder_end_object (builder);
    }

    /* Prompts */
    if (self->prompts)
    {
        json_builder_set_member_name (builder, "prompts");
        json_builder_begin_object (builder);
        if (self->prompts_list_changed)
        {
            json_builder_set_member_name (builder, "listChanged");
            json_builder_add_boolean_value (builder, TRUE);
        }
        json_builder_end_object (builder);
    }

    /* Resources */
    if (self->resources)
    {
        json_builder_set_member_name (builder, "resources");
        json_builder_begin_object (builder);
        if (self->resources_subscribe)
        {
            json_builder_set_member_name (builder, "subscribe");
            json_builder_add_boolean_value (builder, TRUE);
        }
        if (self->resources_list_changed)
        {
            json_builder_set_member_name (builder, "listChanged");
            json_builder_add_boolean_value (builder, TRUE);
        }
        json_builder_end_object (builder);
    }

    /* Tools */
    if (self->tools)
    {
        json_builder_set_member_name (builder, "tools");
        json_builder_begin_object (builder);
        if (self->tools_list_changed)
        {
            json_builder_set_member_name (builder, "listChanged");
            json_builder_add_boolean_value (builder, TRUE);
        }
        json_builder_end_object (builder);
    }

    /* Completions - marker object */
    if (self->completions)
    {
        json_builder_set_member_name (builder, "completions");
        json_builder_begin_object (builder);
        json_builder_end_object (builder);
    }

    /* Tasks - marker object */
    if (self->tasks)
    {
        json_builder_set_member_name (builder, "tasks");
        json_builder_begin_object (builder);
        json_builder_end_object (builder);
    }

    /* Experimental */
    if (g_hash_table_size (self->experimental) > 0)
    {
        json_builder_set_member_name (builder, "experimental");
        json_builder_begin_object (builder);

        g_hash_table_iter_init (&iter, self->experimental);
        while (g_hash_table_iter_next (&iter, &key, &value))
        {
            json_builder_set_member_name (builder, (const gchar *)key);
            json_builder_add_value (builder, json_node_copy ((JsonNode *)value));
        }

        json_builder_end_object (builder);
    }

    json_builder_end_object (builder);

    return json_builder_get_root (builder);
}

/**
 * mcp_server_capabilities_new_from_json:
 * @node: a #JsonNode
 * @error: (nullable): return location for a #GError
 *
 * Creates from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpServerCapabilities, or %NULL
 */
McpServerCapabilities *
mcp_server_capabilities_new_from_json (JsonNode  *node,
                                       GError   **error)
{
    JsonObject *obj;
    McpServerCapabilities *caps;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "ServerCapabilities must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);
    caps = mcp_server_capabilities_new ();

    /* Logging */
    if (json_object_has_member (obj, "logging"))
    {
        caps->logging = TRUE;
    }

    /* Prompts */
    if (json_object_has_member (obj, "prompts"))
    {
        JsonObject *prompts_obj = json_object_get_object_member (obj, "prompts");
        caps->prompts = TRUE;
        if (prompts_obj != NULL && json_object_has_member (prompts_obj, "listChanged"))
        {
            caps->prompts_list_changed = json_object_get_boolean_member (prompts_obj, "listChanged");
        }
    }

    /* Resources */
    if (json_object_has_member (obj, "resources"))
    {
        JsonObject *resources_obj = json_object_get_object_member (obj, "resources");
        caps->resources = TRUE;
        if (resources_obj != NULL)
        {
            if (json_object_has_member (resources_obj, "subscribe"))
            {
                caps->resources_subscribe = json_object_get_boolean_member (resources_obj, "subscribe");
            }
            if (json_object_has_member (resources_obj, "listChanged"))
            {
                caps->resources_list_changed = json_object_get_boolean_member (resources_obj, "listChanged");
            }
        }
    }

    /* Tools */
    if (json_object_has_member (obj, "tools"))
    {
        JsonObject *tools_obj = json_object_get_object_member (obj, "tools");
        caps->tools = TRUE;
        if (tools_obj != NULL && json_object_has_member (tools_obj, "listChanged"))
        {
            caps->tools_list_changed = json_object_get_boolean_member (tools_obj, "listChanged");
        }
    }

    /* Completions */
    if (json_object_has_member (obj, "completions"))
    {
        caps->completions = TRUE;
    }

    /* Tasks */
    if (json_object_has_member (obj, "tasks"))
    {
        caps->tasks = TRUE;
    }

    /* Experimental */
    if (json_object_has_member (obj, "experimental"))
    {
        JsonObject *exp_obj = json_object_get_object_member (obj, "experimental");
        if (exp_obj != NULL)
        {
            GList *members = json_object_get_members (exp_obj);
            GList *l;

            for (l = members; l != NULL; l = l->next)
            {
                const gchar *name = (const gchar *)l->data;
                JsonNode *config = json_object_get_member (exp_obj, name);
                g_hash_table_insert (caps->experimental,
                                     g_strdup (name),
                                     json_node_copy (config));
            }

            g_list_free (members);
        }
    }

    return caps;
}
/* ========================================================================== */
/* McpClientCapabilities                                                      */
/* ========================================================================== */

struct _McpClientCapabilities
{
    GObject parent_instance;

    /* Sampling - marker capability */
    gboolean sampling;

    /* Roots capability */
    gboolean roots;
    gboolean roots_list_changed;

    /* Elicitation - marker capability */
    gboolean elicitation;

    /* Tasks - marker capability (experimental) */
    gboolean tasks;

    /* Experimental capabilities */
    GHashTable *experimental;
};

G_DEFINE_TYPE (McpClientCapabilities, mcp_client_capabilities, G_TYPE_OBJECT)

static void
mcp_client_capabilities_finalize (GObject *object)
{
    McpClientCapabilities *self = MCP_CLIENT_CAPABILITIES (object);

    g_clear_pointer (&self->experimental, g_hash_table_unref);

    G_OBJECT_CLASS (mcp_client_capabilities_parent_class)->finalize (object);
}

static void
mcp_client_capabilities_class_init (McpClientCapabilitiesClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = mcp_client_capabilities_finalize;
}

static void
mcp_client_capabilities_init (McpClientCapabilities *self)
{
    self->experimental = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free,
                                                 (GDestroyNotify)json_node_unref);
}

/**
 * mcp_client_capabilities_new:
 *
 * Creates a new client capabilities object.
 *
 * Returns: (transfer full): a new #McpClientCapabilities
 */
McpClientCapabilities *
mcp_client_capabilities_new (void)
{
    return g_object_new (MCP_TYPE_CLIENT_CAPABILITIES, NULL);
}

/* Sampling */

gboolean
mcp_client_capabilities_get_sampling (McpClientCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_CLIENT_CAPABILITIES (self), FALSE);
    return self->sampling;
}

void
mcp_client_capabilities_set_sampling (McpClientCapabilities *self,
                                      gboolean               enabled)
{
    g_return_if_fail (MCP_IS_CLIENT_CAPABILITIES (self));
    self->sampling = enabled;
}

/* Roots */

gboolean
mcp_client_capabilities_get_roots (McpClientCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_CLIENT_CAPABILITIES (self), FALSE);
    return self->roots;
}

void
mcp_client_capabilities_set_roots (McpClientCapabilities *self,
                                   gboolean               enabled,
                                   gboolean               list_changed)
{
    g_return_if_fail (MCP_IS_CLIENT_CAPABILITIES (self));
    self->roots = enabled;
    self->roots_list_changed = list_changed;
}

gboolean
mcp_client_capabilities_get_roots_list_changed (McpClientCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_CLIENT_CAPABILITIES (self), FALSE);
    return self->roots_list_changed;
}

/* Elicitation */

gboolean
mcp_client_capabilities_get_elicitation (McpClientCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_CLIENT_CAPABILITIES (self), FALSE);
    return self->elicitation;
}

void
mcp_client_capabilities_set_elicitation (McpClientCapabilities *self,
                                         gboolean               enabled)
{
    g_return_if_fail (MCP_IS_CLIENT_CAPABILITIES (self));
    self->elicitation = enabled;
}

/* Tasks */

gboolean
mcp_client_capabilities_get_tasks (McpClientCapabilities *self)
{
    g_return_val_if_fail (MCP_IS_CLIENT_CAPABILITIES (self), FALSE);
    return self->tasks;
}

void
mcp_client_capabilities_set_tasks (McpClientCapabilities *self,
                                   gboolean               enabled)
{
    g_return_if_fail (MCP_IS_CLIENT_CAPABILITIES (self));
    self->tasks = enabled;
}

/* Experimental */

void
mcp_client_capabilities_set_experimental (McpClientCapabilities *self,
                                          const gchar           *name,
                                          JsonNode              *config)
{
    g_return_if_fail (MCP_IS_CLIENT_CAPABILITIES (self));
    g_return_if_fail (name != NULL);

    if (config != NULL)
    {
        g_hash_table_insert (self->experimental,
                             g_strdup (name),
                             json_node_copy (config));
    }
    else
    {
        /* Store empty object to indicate capability presence */
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        json_builder_begin_object (builder);
        json_builder_end_object (builder);
        g_hash_table_insert (self->experimental,
                             g_strdup (name),
                             json_builder_get_root (builder));
    }
}

JsonNode *
mcp_client_capabilities_get_experimental (McpClientCapabilities *self,
                                          const gchar           *name)
{
    g_return_val_if_fail (MCP_IS_CLIENT_CAPABILITIES (self), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (self->experimental, name);
}

/**
 * mcp_client_capabilities_to_json:
 * @self: an #McpClientCapabilities
 *
 * Serializes to JSON.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *
mcp_client_capabilities_to_json (McpClientCapabilities *self)
{
    g_autoptr(JsonBuilder) builder = NULL;
    GHashTableIter iter;
    gpointer key, value;

    g_return_val_if_fail (MCP_IS_CLIENT_CAPABILITIES (self), NULL);

    builder = json_builder_new ();

    json_builder_begin_object (builder);

    /* Sampling - marker object */
    if (self->sampling)
    {
        json_builder_set_member_name (builder, "sampling");
        json_builder_begin_object (builder);
        json_builder_end_object (builder);
    }

    /* Roots */
    if (self->roots)
    {
        json_builder_set_member_name (builder, "roots");
        json_builder_begin_object (builder);
        if (self->roots_list_changed)
        {
            json_builder_set_member_name (builder, "listChanged");
            json_builder_add_boolean_value (builder, TRUE);
        }
        json_builder_end_object (builder);
    }

    /* Elicitation - marker object */
    if (self->elicitation)
    {
        json_builder_set_member_name (builder, "elicitation");
        json_builder_begin_object (builder);
        json_builder_end_object (builder);
    }

    /* Tasks - marker object */
    if (self->tasks)
    {
        json_builder_set_member_name (builder, "tasks");
        json_builder_begin_object (builder);
        json_builder_end_object (builder);
    }

    /* Experimental */
    if (g_hash_table_size (self->experimental) > 0)
    {
        json_builder_set_member_name (builder, "experimental");
        json_builder_begin_object (builder);

        g_hash_table_iter_init (&iter, self->experimental);
        while (g_hash_table_iter_next (&iter, &key, &value))
        {
            json_builder_set_member_name (builder, (const gchar *)key);
            json_builder_add_value (builder, json_node_copy ((JsonNode *)value));
        }

        json_builder_end_object (builder);
    }

    json_builder_end_object (builder);

    return json_builder_get_root (builder);
}

/**
 * mcp_client_capabilities_new_from_json:
 * @node: a #JsonNode
 * @error: (nullable): return location for a #GError
 *
 * Creates from JSON.
 *
 * Returns: (transfer full) (nullable): a new #McpClientCapabilities, or %NULL
 */
McpClientCapabilities *
mcp_client_capabilities_new_from_json (JsonNode  *node,
                                       GError   **error)
{
    JsonObject *obj;
    McpClientCapabilities *caps;

    g_return_val_if_fail (node != NULL, NULL);

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_set_error (error, MCP_ERROR, MCP_ERROR_INVALID_PARAMS,
                     "ClientCapabilities must be a JSON object");
        return NULL;
    }

    obj = json_node_get_object (node);
    caps = mcp_client_capabilities_new ();

    /* Sampling */
    if (json_object_has_member (obj, "sampling"))
    {
        caps->sampling = TRUE;
    }

    /* Roots */
    if (json_object_has_member (obj, "roots"))
    {
        JsonObject *roots_obj = json_object_get_object_member (obj, "roots");
        caps->roots = TRUE;
        if (roots_obj != NULL && json_object_has_member (roots_obj, "listChanged"))
        {
            caps->roots_list_changed = json_object_get_boolean_member (roots_obj, "listChanged");
        }
    }

    /* Elicitation */
    if (json_object_has_member (obj, "elicitation"))
    {
        caps->elicitation = TRUE;
    }

    /* Tasks */
    if (json_object_has_member (obj, "tasks"))
    {
        caps->tasks = TRUE;
    }

    /* Experimental */
    if (json_object_has_member (obj, "experimental"))
    {
        JsonObject *exp_obj = json_object_get_object_member (obj, "experimental");
        if (exp_obj != NULL)
        {
            GList *members = json_object_get_members (exp_obj);
            GList *l;

            for (l = members; l != NULL; l = l->next)
            {
                const gchar *name = (const gchar *)l->data;
                JsonNode *config = json_object_get_member (exp_obj, name);
                g_hash_table_insert (caps->experimental,
                                     g_strdup (name),
                                     json_node_copy (config));
            }

            g_list_free (members);
        }
    }

    return caps;
}
