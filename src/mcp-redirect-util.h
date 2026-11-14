/*
 * mcp-redirect-util.h - Where a credentialed request may be redirected
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Internal helper.  `static inline`, so nothing new is exported.
 *
 * libsoup follows 301/302/303/307/308 by default and copies the request
 * headers onto the new message, so a bearer token or an Mcp-Session-Id
 * set by hand goes wherever the Location header points -- another host
 * included -- and a 307/308 sends the JSON-RPC body there as well.  The
 * transports therefore mark every message that carries a credential
 * SOUP_MESSAGE_NO_REDIRECT, and a 3xx comes back here and is turned into
 * an error that names what to configure instead.
 *
 * The decision is a pure function of (requested URL, status, Location)
 * so a test can reach every branch without a server.
 */

#ifndef MCP_REDIRECT_UTIL_H
#define MCP_REDIRECT_UTIL_H

#include <glib.h>
#include <libsoup/soup.h>

#include "mcp-error.h"

G_BEGIN_DECLS

/*
 * mcp_redirect_status_is_followable:
 * @status: an HTTP status code
 *
 * Exactly the set libsoup's own redirect handler follows.  A 304 is a
 * cache answer and a 300 is never followed automatically, so neither is
 * a redirect anybody's credential could ride.
 *
 * Returns: %TRUE if libsoup would have followed @status
 */
static inline gboolean
mcp_redirect_status_is_followable (guint status)
{
    switch (status)
    {
    case SOUP_STATUS_MOVED_PERMANENTLY:
    case SOUP_STATUS_FOUND:
    case SOUP_STATUS_SEE_OTHER:
    case SOUP_STATUS_TEMPORARY_REDIRECT:
    case SOUP_STATUS_PERMANENT_REDIRECT:
        return TRUE;
    default:
        return FALSE;
    }
}

/*
 * mcp_redirect_uri_origin:
 * @uri: a parsed URI
 *
 * scheme://host[:port], and nothing after it.  The path and query are
 * left out on purpose: they can carry a token or a code, and this
 * string goes into a GError that ends up in logs.
 *
 * Returns: (transfer full): the origin
 */
static inline gchar *
mcp_redirect_uri_origin (GUri *uri)
{
    const gchar *scheme;
    const gchar *host;

    scheme = g_uri_get_scheme (uri);
    host = g_uri_get_host (uri);

    /* g_uri_join() brackets an IPv6 literal, which a printf would not. */
    return g_uri_join (G_URI_FLAGS_NONE,
                       scheme != NULL ? scheme : "http",
                       NULL,
                       host != NULL ? host : "",
                       g_uri_get_port (uri),
                       "",
                       NULL,
                       NULL);
}

/*
 * mcp_redirect_refusal:
 * @requested_url: (nullable): the URL the request went to
 * @status: the status it was answered with
 * @location: (nullable): the response's Location header
 *
 * Decides whether a credentialed request's answer is a redirect it was
 * refused, and if so says where to point the transport instead.  Only
 * origins are named, never a path or query from another host; on the
 * same origin the target path is named, since it is the one to set.
 *
 * Returns: (transfer full) (nullable): an %MCP_ERROR_TRANSPORT_ERROR,
 *   or %NULL when @status is not a redirect libsoup would follow
 */
static inline GError *
mcp_redirect_refusal (const gchar *requested_url,
                      guint        status,
                      const gchar *location)
{
    g_autoptr(GUri) requested = NULL;
    g_autoptr(GUri) target = NULL;
    g_autofree gchar *from = NULL;
    g_autofree gchar *to = NULL;

    if (!mcp_redirect_status_is_followable (status))
    {
        return NULL;
    }

    requested = requested_url != NULL
        ? g_uri_parse (requested_url, G_URI_FLAGS_NONE, NULL)
        : NULL;
    from = requested != NULL
        ? mcp_redirect_uri_origin (requested)
        : g_strdup ("the server");

    /* A relative Location is legal (RFC 9110) and common. */
    if (location != NULL && location[0] != '\0')
    {
        if (requested != NULL)
        {
            target = g_uri_parse_relative (requested, location,
                                           G_URI_FLAGS_NONE, NULL);
        }
        else
        {
            target = g_uri_parse (location, G_URI_FLAGS_NONE, NULL);
        }
    }

    if (target == NULL)
    {
        return g_error_new (MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                            "%s answered %u, a redirect to %s; mcp-glib "
                            "never sends a credential across a redirect, "
                            "so the request was not repeated -- check the "
                            "URL configured for it",
                            from, status,
                            (location != NULL && location[0] != '\0')
                                ? "an address that does not parse"
                                : "nowhere (no Location header)");
    }

    to = mcp_redirect_uri_origin (target);

    /*
     * Same origin still refuses: a trailing slash or a moved endpoint is
     * the usual cause, and the fix -- the path it names -- is the same.
     * The path is safe to name because it is on a host the caller
     * already configured.
     */
    if (g_strcmp0 (from, to) == 0)
    {
        return g_error_new (MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                            "%s answered %u, a redirect to %s on the same "
                            "server; mcp-glib never sends a credential "
                            "across a redirect, so the request was not "
                            "repeated -- configure that path instead",
                            from, status, g_uri_get_path (target));
    }

    return g_error_new (MCP_ERROR, MCP_ERROR_TRANSPORT_ERROR,
                        "%s answered %u, a redirect to %s; mcp-glib never "
                        "sends a credential across a redirect, so the "
                        "request was not repeated -- if %s is where it "
                        "should go, configure that address instead",
                        from, status, to, to);
}

/*
 * mcp_redirect_check:
 * @message: a message that has been answered
 *
 * The refusal for @message, if it was sent with
 * %SOUP_MESSAGE_NO_REDIRECT and answered with a redirect.  A message
 * that was allowed to follow redirects already has, so it never gets
 * one.
 *
 * Returns: (transfer full) (nullable): the refusal, or %NULL
 */
static inline GError *
mcp_redirect_check (SoupMessage *message)
{
    g_autofree gchar *requested_url = NULL;

    if (message == NULL)
    {
        return NULL;
    }

    if ((soup_message_get_flags (message) & SOUP_MESSAGE_NO_REDIRECT) == 0)
    {
        return NULL;
    }

    requested_url = g_uri_to_string (soup_message_get_uri (message));

    return mcp_redirect_refusal (
        requested_url,
        soup_message_get_status (message),
        soup_message_headers_get_one (
            soup_message_get_response_headers (message), "Location"));
}

G_END_DECLS

#endif /* MCP_REDIRECT_UTIL_H */
