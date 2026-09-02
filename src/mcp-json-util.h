/*
 * mcp-json-util.h - Type-checked JSON member accessors for mcp-glib
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Internal helpers.  Not part of the installed API surface: every symbol
 * here is `static inline`, so nothing new is exported and nothing new is
 * introspected.
 *
 * Two things every one of these exists to stop.
 *
 * json_object_has_member() answers "is the key present", which is a
 * different question from "is the value the type I am about to read it
 * as".  json_object_get_string_member() on a number returns NULL without
 * a warning, and that NULL then travels: g_hash_table_contains() on a
 * g_str_hash table dereferences it, so
 * {"method":"tools/call","params":{"name":123}} was a remote segfault.
 * The object and array accessors are no better -- they log a Json-CRITICAL
 * and hand back NULL, which a caller checking only has_member never
 * expects.
 *
 * json_node_new (JSON_NODE_OBJECT) makes a node that answers
 * JSON_NODE_HOLDS_OBJECT() with TRUE and json_node_get_object() with
 * NULL.  It is not an empty object, it is an object-shaped hole, and
 * serialising one walks into json_object_get_members_internal() with a
 * NULL object and dies.  mcp_json_new_empty_object() is the spelling
 * that produces {}.
 */

#ifndef MCP_JSON_UTIL_H
#define MCP_JSON_UTIL_H

#include <glib.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/*
 * mcp_json_get_member_of_type:
 *
 * Returns the member's node only when it is present and actually holds
 * @type; NULL otherwise.  JSON null is deliberately treated as absent --
 * a peer that sends "mimeType": null means the same thing as omitting it,
 * and every caller here would otherwise have to spell that out again.
 */
static inline JsonNode *
mcp_json_get_member_of_type (JsonObject   *obj,
                             const gchar  *member,
                             JsonNodeType  type)
{
    JsonNode *node;

    if (obj == NULL || member == NULL)
    {
        return NULL;
    }

    node = json_object_get_member (obj, member);
    if (node == NULL || json_node_get_node_type (node) != type)
    {
        return NULL;
    }

    return node;
}

/*
 * mcp_json_get_string:
 *
 * The member's string value, or NULL when the member is absent, is JSON
 * null, or holds any other type.  Never returns a pointer into a value of
 * the wrong type, and never logs.
 */
static inline const gchar *
mcp_json_get_string (JsonObject  *obj,
                     const gchar *member)
{
    JsonNode *node;

    node = mcp_json_get_member_of_type (obj, member, JSON_NODE_VALUE);
    if (node == NULL || json_node_get_value_type (node) != G_TYPE_STRING)
    {
        return NULL;
    }

    return json_node_get_string (node);
}

/*
 * mcp_json_has_string:
 *
 * TRUE when the member is present and is a JSON string.  For the callers
 * that must distinguish "absent" from "empty string".
 */
static inline gboolean
mcp_json_has_string (JsonObject  *obj,
                     const gchar *member)
{
    return mcp_json_get_string (obj, member) != NULL;
}

/*
 * mcp_json_get_object:
 *
 * The member's object, or NULL when absent or of another type.  Unlike
 * json_object_get_object_member() this does not log a Json-CRITICAL for a
 * peer sending the wrong type, which is a remote log-flooding lever.
 */
static inline JsonObject *
mcp_json_get_object (JsonObject  *obj,
                     const gchar *member)
{
    JsonNode *node;

    node = mcp_json_get_member_of_type (obj, member, JSON_NODE_OBJECT);
    if (node == NULL)
    {
        return NULL;
    }

    return json_node_get_object (node);
}

/*
 * mcp_json_get_array:
 *
 * The member's array, or NULL when absent or of another type.
 */
static inline JsonArray *
mcp_json_get_array (JsonObject  *obj,
                    const gchar *member)
{
    JsonNode *node;

    node = mcp_json_get_member_of_type (obj, member, JSON_NODE_ARRAY);
    if (node == NULL)
    {
        return NULL;
    }

    return json_node_get_array (node);
}

/*
 * mcp_json_get_boolean:
 *
 * The member's boolean value, or @fallback when the member is absent or
 * is not a JSON boolean.  A JSON boolean read with the string reader (or
 * the other way round) fails to the fallback in silence, so the type is
 * checked rather than assumed.
 */
static inline gboolean
mcp_json_get_boolean (JsonObject  *obj,
                      const gchar *member,
                      gboolean     fallback)
{
    JsonNode *node;

    node = mcp_json_get_member_of_type (obj, member, JSON_NODE_VALUE);
    if (node == NULL || json_node_get_value_type (node) != G_TYPE_BOOLEAN)
    {
        return fallback;
    }

    return json_node_get_boolean (node);
}

/*
 * mcp_json_get_int:
 *
 * The member's integer value, or @fallback when absent or not a number.
 * A JSON document that went through a JavaScript encoder may spell a
 * whole number as a double, so both are accepted.
 */
static inline gint64
mcp_json_get_int (JsonObject  *obj,
                  const gchar *member,
                  gint64       fallback)
{
    JsonNode *node;
    GType value_type;

    node = mcp_json_get_member_of_type (obj, member, JSON_NODE_VALUE);
    if (node == NULL)
    {
        return fallback;
    }

    value_type = json_node_get_value_type (node);
    if (value_type == G_TYPE_INT64)
    {
        return json_node_get_int (node);
    }
    if (value_type == G_TYPE_DOUBLE)
    {
        return (gint64) json_node_get_double (node);
    }

    return fallback;
}

/*
 * mcp_json_get_double:
 *
 * The member's floating-point value, or @fallback when absent or not a
 * number.  An integer is a number too -- json-glib types 1 as G_TYPE_INT64
 * and 1.0 as G_TYPE_DOUBLE, and a peer is entitled to send either.
 */
static inline gdouble
mcp_json_get_double (JsonObject  *obj,
                     const gchar *member,
                     gdouble      fallback)
{
    JsonNode *node;
    GType value_type;

    node = mcp_json_get_member_of_type (obj, member, JSON_NODE_VALUE);
    if (node == NULL)
    {
        return fallback;
    }

    value_type = json_node_get_value_type (node);
    if (value_type == G_TYPE_DOUBLE)
    {
        return json_node_get_double (node);
    }
    if (value_type == G_TYPE_INT64)
    {
        return (gdouble) json_node_get_int (node);
    }

    return fallback;
}

/*
 * mcp_json_array_get_object:
 *
 * Element @index of @array when it is an object, NULL otherwise.
 * json_array_get_object_element() logs and returns NULL for an element of
 * the wrong type, so a peer can flood the log with [1,1,1,...].
 */
static inline JsonObject *
mcp_json_array_get_object (JsonArray *array,
                           guint      index)
{
    JsonNode *node;

    if (array == NULL || index >= json_array_get_length (array))
    {
        return NULL;
    }

    node = json_array_get_element (array, index);
    if (node == NULL || !JSON_NODE_HOLDS_OBJECT (node))
    {
        return NULL;
    }

    return json_node_get_object (node);
}

/*
 * mcp_json_new_empty_object:
 *
 * A #JsonNode holding a real, empty #JsonObject -- the thing
 * json_node_new (JSON_NODE_OBJECT) looks like it produces and does not.
 *
 * Returns: (transfer full): a node that serialises as {}
 */
static inline JsonNode *
mcp_json_new_empty_object (void)
{
    JsonNode *node;
    JsonObject *obj;

    node = json_node_new (JSON_NODE_OBJECT);
    obj = json_object_new ();
    json_node_take_object (node, obj);

    return node;
}

G_END_DECLS

#endif /* MCP_JSON_UTIL_H */
