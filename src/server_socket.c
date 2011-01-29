/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#ifdef HAVE_STRUCT_UCRED
#define _GNU_SOURCE 1
#endif

#include "server_socket.h"
#include "socket_util.h"
#include "fd_util.h"
#include "glib_compat.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#ifdef WIN32
#define WINVER 0x0501
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "listen"

#define DEFAULT_PORT	6600

struct one_socket {
	struct one_socket *next;
	struct server_socket *parent;

	unsigned serial;

	int fd;
	guint source_id;

	char *path;

	size_t address_length;
	struct sockaddr address;
};

struct server_socket {
	server_socket_callback_t callback;
	void *callback_ctx;

	struct one_socket *sockets, **sockets_tail_r;
	unsigned next_serial;
};

static GQuark
server_socket_quark(void)
{
	return g_quark_from_static_string("server_socket");
}

struct server_socket *
server_socket_new(server_socket_callback_t callback, void *callback_ctx)
{
	struct server_socket *ss = g_new(struct server_socket, 1);
	ss->callback = callback;
	ss->callback_ctx = callback_ctx;
	ss->sockets = NULL;
	ss->sockets_tail_r = &ss->sockets;
	ss->next_serial = 1;
	return ss;
}

void
server_socket_free(struct server_socket *ss)
{
	server_socket_close(ss);

	while (ss->sockets != NULL) {
		struct one_socket *s = ss->sockets;
		ss->sockets = s->next;

		assert(s->fd < 0);

		g_free(s->path);
		g_free(s);
	}

	g_free(ss);
}

/**
 * Wraper for sockaddr_to_string() which never fails.
 */
static char *
one_socket_to_string(const struct one_socket *s)
{
	char *p = sockaddr_to_string(&s->address, s->address_length, NULL);
	if (p == NULL)
		p = g_strdup("[unknown]");
	return p;
}

static int
get_remote_uid(int fd)
{
#ifdef HAVE_STRUCT_UCRED
	struct ucred cred;
	socklen_t len = sizeof (cred);

	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0)
		return 0;

	return cred.uid;
#else
#ifdef HAVE_GETPEEREID
	uid_t euid;
	gid_t egid;

	if (getpeereid(fd, &euid, &egid) == 0)
		return euid;
#else
	(void)fd;
#endif
	return -1;
#endif
}

static gboolean
server_socket_in_event(G_GNUC_UNUSED GIOChannel *source,
		       G_GNUC_UNUSED GIOCondition condition,
		       gpointer data)
{
	struct one_socket *s = data;

	struct sockaddr_storage address;
	size_t address_length = sizeof(address);
	int fd = accept_cloexec_nonblock(s->fd, (struct sockaddr*)&address,
					 &address_length);
	if (fd >= 0)
		s->parent->callback(fd, (const struct sockaddr*)&address,
				    address_length, get_remote_uid(fd),
				    s->parent->callback_ctx);
	else
		g_warning("accept() failed: %s", g_strerror(errno));

	return true;
}

bool
server_socket_open(struct server_socket *ss, GError **error_r)
{
	struct one_socket *good = NULL, *bad = NULL;
	GError *last_error = NULL;

	for (struct one_socket *s = ss->sockets; s != NULL; s = s->next) {
		assert(s->serial > 0);
		assert(good == NULL || s->serial >= good->serial);
		assert(s->fd < 0);

		if (bad != NULL && s->serial != bad->serial) {
			server_socket_close(ss);
			g_propagate_error(error_r, last_error);
			return false;
		}

		GError *error = NULL;
		s->fd = socket_bind_listen(s->address.sa_family, SOCK_STREAM, 0,
					   &s->address, s->address_length, 5,
					   &error);
		if (s->fd < 0) {
			if (good != NULL && good->serial == s->serial) {
				char *address_string = one_socket_to_string(s);
				char *good_string = one_socket_to_string(good);
				g_warning("bind to '%s' failed: %s "
					  "(continuing anyway, because "
					  "binding to '%s' succeeded)",
					  address_string, error->message,
					  good_string);
				g_free(address_string);
				g_free(good_string);
				g_error_free(error);
			} else if (bad == NULL) {
				bad = s;

				char *address_string = one_socket_to_string(s);
				g_propagate_prefixed_error(&last_error, error,
							   "Failed to bind to '%s': ",
							   address_string);
				g_free(address_string);
			} else
				g_error_free(error);
			continue;
		}

		/* allow everybody to connect */

		if (s->path != NULL)
			chmod(s->path, 0666);

		/* register in the GLib main loop */

		GIOChannel *channel = g_io_channel_unix_new(s->fd);
		s->source_id = g_io_add_watch(channel, G_IO_IN,
					      server_socket_in_event, s);
		g_io_channel_unref(channel);

		/* mark this socket as "good", and clear previous
		   errors */

		good = s;

		if (bad != NULL) {
			bad = NULL;
			g_error_free(last_error);
			last_error = NULL;
		}
	}

	if (bad != NULL) {
		server_socket_close(ss);
		g_propagate_error(error_r, last_error);
		return false;
	}

	return true;
}

void
server_socket_close(struct server_socket *ss)
{
	for (struct one_socket *s = ss->sockets; s != NULL; s = s->next) {
		if (s->fd < 0)
			continue;

		g_source_remove(s->source_id);
		close(s->fd);
		s->fd = -1;
	}
}

static struct one_socket *
one_socket_new(unsigned serial, const struct sockaddr *address,
	       size_t address_length)
{
	assert(address != NULL);
	assert(address_length > 0);

	struct one_socket *s = g_malloc(sizeof(*s) - sizeof(s->address) +
					address_length);
	s->next = NULL;
	s->serial = serial;
	s->fd = -1;
	s->path = NULL;
	s->address_length = address_length;
	memcpy(&s->address, address, address_length);

	return s;
}

static struct one_socket *
server_socket_add_address(struct server_socket *ss,
			  const struct sockaddr *address,
			  size_t address_length)
{
	assert(ss != NULL);
	assert(ss->sockets_tail_r != NULL);
	assert(*ss->sockets_tail_r == NULL);

	struct one_socket *s = one_socket_new(ss->next_serial,
					      address, address_length);
	s->parent = ss;
	*ss->sockets_tail_r = s;
	ss->sockets_tail_r = &s->next;

	return s;
}

#ifdef HAVE_TCP

/**
 * Add a listener on a port on all IPv4 interfaces.
 *
 * @param port the TCP port
 */
static void
server_socket_add_port_ipv4(struct server_socket *ss, unsigned port)
{
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

	server_socket_add_address(ss, (const struct sockaddr *)&sin,
				  sizeof(sin));
}

#ifdef HAVE_IPV6
/**
 * Add a listener on a port on all IPv6 interfaces.
 *
 * @param port the TCP port
 */
static void
server_socket_add_port_ipv6(struct server_socket *ss, unsigned port)
{
	struct sockaddr_in6 sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin6_port = htons(port);
	sin.sin6_family = AF_INET6;

	server_socket_add_address(ss, (const struct sockaddr *)&sin,
				  sizeof(sin));
}
#endif /* HAVE_IPV6 */

#endif /* HAVE_TCP */

bool
server_socket_add_port(struct server_socket *ss, unsigned port,
		       GError **error_r)
{
#ifdef HAVE_TCP
	if (port == 0 || port > 0xffff) {
		g_set_error(error_r, server_socket_quark(), 0,
			    "Invalid TCP port");
		return false;
	}

#ifdef HAVE_IPV6
	server_socket_add_port_ipv6(ss, port);
#endif
	server_socket_add_port_ipv4(ss, port);

	++ss->next_serial;

	return true;
#else /* HAVE_TCP */
	(void)ss;
	(void)port;

	g_set_error(error_r, server_socket_quark(), 0,
		    "TCP support is disabled");
	return false;
#endif /* HAVE_TCP */
}

bool
server_socket_add_host(struct server_socket *ss, const char *hostname,
		       unsigned port, GError **error_r)
{
#ifdef HAVE_TCP
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char service[20];
	g_snprintf(service, sizeof(service), "%u", port);

	struct addrinfo *ai;
	int ret = getaddrinfo(hostname, service, &hints, &ai);
	if (ret != 0) {
		g_set_error(error_r, server_socket_quark(), ret,
			    "Failed to look up host \"%s\": %s",
			    hostname, gai_strerror(ret));
		return false;
	}

	for (const struct addrinfo *i = ai; i != NULL; i = i->ai_next)
		server_socket_add_address(ss, i->ai_addr, i->ai_addrlen);

	freeaddrinfo(ai);

	++ss->next_serial;

	return true;
#else /* HAVE_TCP */
	(void)ss;
	(void)hostname;
	(void)port;

	g_set_error(error_r, server_socket_quark(), 0,
		    "TCP support is disabled");
	return false;
#endif /* HAVE_TCP */
}

bool
server_socket_add_path(struct server_socket *ss, const char *path,
		       GError **error_r)
{
#ifdef HAVE_UN
	struct sockaddr_un s_un;

	size_t path_length = strlen(path);
	if (path_length >= sizeof(s_un.sun_path)) {
		g_set_error(error_r, server_socket_quark(), 0,
			    "UNIX socket path is too long");
		return false;
	}

	unlink(path);

	s_un.sun_family = AF_UNIX;
	memcpy(s_un.sun_path, path, path_length + 1);

	struct one_socket *s =
		server_socket_add_address(ss, (const struct sockaddr *)&s_un,
					  sizeof(s_un));
	s->path = g_strdup(path);

	return true;
#else /* !HAVE_UN */
	(void)ss;
	(void)path;

	g_set_error(error_r, server_socket_quark(), 0,
		    "UNIX domain socket support is disabled");
	return false;
#endif /* !HAVE_UN */
}

