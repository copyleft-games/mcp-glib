/*
 * mcp-listen-util.h - Binding a SoupServer to the host it was told to
 *
 * Copyright (C) 2026 Copyleft Games
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Internal helper.  `static inline`, so nothing new is exported.
 *
 * Both server transports had the same shape: 127.0.0.1 and "localhost"
 * went to soup_server_listen_local(), and *anything else* went to
 * soup_server_listen_all() under a comment promising the handler would
 * filter.  No filter was ever written, and require-auth defaults to
 * FALSE -- so naming one interface published the server on every network
 * the machine is on, which is the opposite of what naming an interface
 * asks for.  A wall built without the door the check says is there.
 *
 * There is nothing to filter *for*: soup_server_listen() takes a
 * GSocketAddress and the kernel does the binding.  A name that does not
 * resolve is an error, not a reason to widen.
 */

#ifndef MCP_LISTEN_UTIL_H
#define MCP_LISTEN_UTIL_H

#include <gio/gio.h>
#include <libsoup/soup.h>

#include "mcp-error.h"

G_BEGIN_DECLS

/*
 * mcp_server_listen_on_host:
 * @server: the #SoupServer to bind
 * @host: (nullable): the address or name to bind to; %NULL or "" means
 *   every interface, which is the documented default
 * @port: the port, or 0 to let the kernel choose
 * @error: (nullable): return location for a #GError
 *
 * Returns: %TRUE if the server is listening on exactly what @host names
 */
static inline gboolean
mcp_server_listen_on_host (SoupServer   *server,
                           const gchar  *host,
                           guint         port,
                           GError      **error)
{
    g_autoptr(GSocketAddress) address = NULL;
    g_autoptr(GInetAddress) inet_address = NULL;
    g_autoptr(GResolver) resolver = NULL;
    GList *addresses;

    if (host == NULL || host[0] == '\0')
    {
        return soup_server_listen_all (server, port, 0, error);
    }

    /* soup_server_listen_local() binds both loopback families, which is
     * what somebody naming localhost means and what an address literal
     * cannot express. */
    if (g_strcmp0 (host, "localhost") == 0)
    {
        return soup_server_listen_local (server, port, 0, error);
    }

    inet_address = g_inet_address_new_from_string (host);

    if (inet_address == NULL)
    {
        /*
         * A name rather than a literal.  Resolving it is the only way to
         * bind it; refusing outright would break an existing caller that
         * names a host from /etc/hosts, and widening to every interface
         * is what this function exists to stop.
         */
        resolver = g_resolver_get_default ();
        addresses = g_resolver_lookup_by_name (resolver, host, NULL, error);
        if (addresses == NULL)
        {
            return FALSE;
        }

        inet_address = g_object_ref (addresses->data);
        g_resolver_free_addresses (addresses);
    }

    address = g_inet_socket_address_new (inet_address, (guint16) port);

    return soup_server_listen (server, address, 0, error);
}

G_END_DECLS

#endif /* MCP_LISTEN_UTIL_H */
