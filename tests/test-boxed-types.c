/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "mcp.h"

/* Exercise the same copy/free ownership path used by GI boxed wrappers. */
static void
assert_boxed_lifetime (GType type, gpointer instance)
{
	GValue value = G_VALUE_INIT;
	gpointer retained;

	g_assert_true (G_TYPE_IS_BOXED (type));
	g_assert_nonnull (instance);
	g_value_init (&value, type);
	g_value_set_boxed (&value, instance);
	g_assert_true (g_value_get_boxed (&value) == instance);
	g_boxed_free (type, instance);

	/* The GValue reference must outlive the original caller's reference. */
	retained = g_value_dup_boxed (&value);
	g_assert_true (retained == instance);
	g_value_unset (&value);
	g_boxed_free (type, retained);
}

/* Every refcounted protocol record must be usable as a boxed GI value. */
static void
test_boxed_lifetimes (void)
{
	assert_boxed_lifetime (MCP_TYPE_TOOL_RESULT, mcp_tool_result_new (FALSE));
	assert_boxed_lifetime (MCP_TYPE_RESOURCE_CONTENTS,
		mcp_resource_contents_new_text ("file:///test", "body", "text/plain"));
	assert_boxed_lifetime (MCP_TYPE_PROMPT_MESSAGE,
		mcp_prompt_message_new (MCP_ROLE_USER));
	assert_boxed_lifetime (MCP_TYPE_PROMPT_RESULT, mcp_prompt_result_new ("test"));
	assert_boxed_lifetime (MCP_TYPE_SAMPLING_MESSAGE,
		mcp_sampling_message_new (MCP_ROLE_USER));
	assert_boxed_lifetime (MCP_TYPE_MODEL_PREFERENCES, mcp_model_preferences_new ());
	assert_boxed_lifetime (MCP_TYPE_SAMPLING_RESULT,
		mcp_sampling_result_new (MCP_ROLE_ASSISTANT));
	assert_boxed_lifetime (MCP_TYPE_ROOT, mcp_root_new ("file:///test"));
	assert_boxed_lifetime (MCP_TYPE_COMPLETION_RESULT, mcp_completion_result_new ());
}

/* Registered error values must preserve both wire and local error codes. */
static void
test_error_enum (void)
{
	GEnumClass *enum_class;
	GEnumValue *value;
	gint code;

	enum_class = (GEnumClass *) g_type_class_ref (MCP_TYPE_ERROR_CODE);
	g_assert_cmpuint (enum_class->n_values, ==, 17);
	value = g_enum_get_value (enum_class, MCP_ERROR_PARSE_ERROR);
	g_assert_nonnull (value);
	g_assert_cmpstr (value->value_name, ==, "MCP_ERROR_PARSE_ERROR");
	for (code = MCP_ERROR_PROTOCOL_VERSION_MISMATCH;
		 code <= MCP_ERROR_CAPABILITY_NOT_SUPPORTED; code++)
		g_assert_nonnull (g_enum_get_value (enum_class, code));
	g_type_class_unref (enum_class);
}

/* Register the ownership and enum regressions with the normal GTest suite. */
int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/mcp/boxed/lifetimes", test_boxed_lifetimes);
	g_test_add_func ("/mcp/boxed/error-enum", test_error_enum);
	return g_test_run ();
}
