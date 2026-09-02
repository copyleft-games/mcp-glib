/*
 * mcp-backoff-util.h - Exponential reconnect backoff for mcp-glib
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Internal helper.  `static inline`, so nothing new is exported.
 *
 * It lives in a header of its own because the arithmetic is the whole
 * bug and it was unreachable from a test: the computation sat inside a
 * static schedule_reconnect(), and reaching attempt 33 through the public
 * API means thirty-three real failed connections.
 */

#ifndef MCP_BACKOFF_UTIL_H
#define MCP_BACKOFF_UTIL_H

#include <glib.h>

G_BEGIN_DECLS

/*
 * MCP_BACKOFF_MAX_SHIFT:
 *
 * The largest exponent the doubling is allowed to reach.  2^20 times any
 * sane base is far past every cap anybody would set, and stopping here
 * keeps the shift inside a guint whatever the attempt count is.
 */
#define MCP_BACKOFF_MAX_SHIFT (20)

/*
 * mcp_backoff_delay_ms:
 * @base_ms: the first delay
 * @attempt: which attempt this is, counting from 1
 * @cap_ms: the longest delay to return
 *
 * Exponential backoff, base_ms * 2^(attempt-1), clamped to @cap_ms.
 *
 * The shift is bounded, not just the product.  max-reconnect-attempts
 * defaults to 0 -- unlimited -- so the exponent grows without end:
 * `1 << 31` on a signed int is undefined behaviour that UBSan traps, and
 * by attempt 33 the shift wraps to 0.  A zero-millisecond delay
 * reconnects as fast as the main loop can turn, which is the opposite of
 * a backoff and reads as the network flapping rather than as us.
 *
 * Returns: the delay in milliseconds, never 0 unless @base_ms is 0
 */
static inline guint
mcp_backoff_delay_ms (guint base_ms,
                      guint attempt,
                      guint cap_ms)
{
    guint shift;
    guint delay;

    if (base_ms == 0)
    {
        return 0;
    }

    shift = attempt > 0 ? attempt - 1 : 0;
    if (shift > MCP_BACKOFF_MAX_SHIFT)
    {
        shift = MCP_BACKOFF_MAX_SHIFT;
    }

    delay = base_ms << shift;

    /* The multiply can still overflow a guint for a large base; an
     * overflowed product is smaller than the base it started from. */
    if (delay < base_ms || delay > cap_ms)
    {
        delay = cap_ms;
    }

    return delay;
}

G_END_DECLS

#endif /* MCP_BACKOFF_UTIL_H */
