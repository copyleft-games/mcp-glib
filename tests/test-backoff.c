/*
 * test-backoff.c - Reconnect backoff arithmetic
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * The WebSocket transport's backoff lived inside a static
 * schedule_reconnect(), so reaching the attempt where it broke meant
 * thirty-three real failed connections.  It is a pure function now, and
 * this is the attempt count nothing could get to.
 */

#define MCP_COMPILATION
#include "mcp-backoff-util.h"
#undef MCP_COMPILATION

static void
test_first_attempt_is_the_base (void)
{
    g_assert_cmpuint (mcp_backoff_delay_ms (1000, 1, 30000), ==, 1000);
}

static void
test_it_doubles (void)
{
    g_assert_cmpuint (mcp_backoff_delay_ms (1000, 2, 30000), ==, 2000);
    g_assert_cmpuint (mcp_backoff_delay_ms (1000, 3, 30000), ==, 4000);
    g_assert_cmpuint (mcp_backoff_delay_ms (1000, 4, 30000), ==, 8000);
    g_assert_cmpuint (mcp_backoff_delay_ms (1000, 5, 30000), ==, 16000);
}

static void
test_it_clamps (void)
{
    g_assert_cmpuint (mcp_backoff_delay_ms (1000, 6, 30000), ==, 30000);
    g_assert_cmpuint (mcp_backoff_delay_ms (1000, 20, 30000), ==, 30000);
}

/*
 * max-reconnect-attempts defaults to 0, which means never stop, so the
 * exponent is unbounded.  `1 << 31` on a signed int is undefined
 * behaviour; at attempt 33 the shift wrapped to 0 and the delay with it,
 * turning the backoff into a hot loop.  Every one of these must still be
 * the cap.
 */
static void
test_a_high_attempt_count_still_backs_off (void)
{
    guint attempt;

    for (attempt = 30; attempt <= 70; attempt++)
    {
        g_assert_cmpuint (mcp_backoff_delay_ms (1000, attempt, 30000), ==, 30000);
    }

    g_assert_cmpuint (mcp_backoff_delay_ms (1000, G_MAXUINT, 30000), ==, 30000);
}

/* A base large enough that the product overflows a guint. */
static void
test_an_overflowing_base_clamps (void)
{
    g_assert_cmpuint (mcp_backoff_delay_ms (G_MAXUINT / 2, 8, 30000), ==, 30000);
}

static void
test_a_zero_base_stays_zero (void)
{
    g_assert_cmpuint (mcp_backoff_delay_ms (0, 5, 30000), ==, 0);
}

static void
test_attempt_zero_is_the_base (void)
{
    g_assert_cmpuint (mcp_backoff_delay_ms (1000, 0, 30000), ==, 1000);
}

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/mcp/backoff/first-attempt-is-the-base",
                     test_first_attempt_is_the_base);
    g_test_add_func ("/mcp/backoff/it-doubles", test_it_doubles);
    g_test_add_func ("/mcp/backoff/it-clamps", test_it_clamps);
    g_test_add_func ("/mcp/backoff/high-attempt-count-still-backs-off",
                     test_a_high_attempt_count_still_backs_off);
    g_test_add_func ("/mcp/backoff/overflowing-base-clamps",
                     test_an_overflowing_base_clamps);
    g_test_add_func ("/mcp/backoff/zero-base-stays-zero",
                     test_a_zero_base_stays_zero);
    g_test_add_func ("/mcp/backoff/attempt-zero-is-the-base",
                     test_attempt_zero_is_the_base);

    return g_test_run ();
}
