/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "ServerSocket.hxx"
#include "socket_util.h"
#include "resolver.h"
#include "fd_util.h"
#include "glib_socket.h"

#include <forward_list>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#ifdef WIN32
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

struct OneServerSocket {
	const server_socket &parent;

	const unsigned serial;

	int fd;
	guint source_id;

	char *path;

	size_t address_length;
	struct sockaddr *address;

	OneServerSocket(const server_socket &_parent, unsigned _serial,
			const struct sockaddr *_address,
			size_t _address_length)
		:parent(_parent), serial(_serial),
		 fd(-1),
		 path(nullptr),
		 address_length(_address_length),
		 address((sockaddr *)g_memdup(_address, _address_length))
	{
		assert(_address != nullptr);
		assert(_address_length > 0);
	}

	OneServerSocket(const OneServerSocket &other) = delete;
	OneServerSocket &operator=(const OneServerSocket &other) = delete;

	~OneServerSocket() {
		Close();
		g_free(path);
		g_free(address);
	}

	bool Open(GError **error_r);

	void Close();

	char *ToString() const;

	void SetFD(int fd);
};

struct server_socket {
	server_socket_callback_t callback;
	void *callback_ctx;

	std::forward_list<OneServerSocket> sockets;

	unsigned next_serial;

	server_socket(server_socket_callback_t _callback, void *_callback_ctx)
		:callback(_callback), callback_ctx(_callback_ctx),
		 next_serial(1) {}

	void Close();
};

static GQuark
server_socket_quark(void)
{
	return g_quark_from_static_string("server_socket");
}

struct server_socket *
server_socket_new(server_socket_callback_t callback, void *callback_ctx)
{
	return new server_socket(callback, callback_ctx);
}

void
server_socket_free(struct server_socket *ss)
{
	delete ss;
}

/**
 * Wraper for sockaddr_to_string() which never fails.
 */
char *
OneServerSocket::ToString() const
{
	char *p = sockaddr_to_string(address, address_length, nullptr);
	if (p == nullptr)
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
	OneServerSocket *s = (OneServerSocket *)data;

	struct sockaddr_storage address;
	size_t address_length = sizeof(address);
	int fd = accept_cloexec_nonblock(s->fd, (struct sockaddr*)&address,
					 &address_length);
	if (fd >= 0) {
		if (socket_keepalive(fd))
			g_warning("Could not set TCP keepalive option: %s",
				  g_strerror(errno));
		s->parent.callback(fd, (const struct sockaddr*)&address,
				   address_length, get_remote_uid(fd),
				   s->parent.callback_ctx);
	} else {
		g_warning("accept() failed: %s", g_strerror(errno));
	}

	return true;
}

void
OneServerSocket::SetFD(int _fd)
{
	assert(fd < 0);
	assert(_fd >= 0);

	fd = _fd;

	GIOChannel *channel = g_io_channel_new_socket(fd);
	source_id = g_io_add_watch(channel, G_IO_IN,
				   server_socket_in_event, this);
	g_io_channel_unref(channel);
}

inline bool
OneServerSocket::Open(GError **error_r)
{
	assert(fd < 0);

	int _fd = socket_bind_listen(address->sa_family,
				     SOCK_STREAM, 0,
				     address, address_length, 5,
				     error_r);
	if (_fd < 0)
		return false;

	/* allow everybody to connect */

	if (path != nullptr)
		chmod(path, 0666);

	/* register in the GLib main loop */

	SetFD(_fd);

	return true;
}

bool
server_socket_open(struct server_socket *ss, GError **error_r)
{
	struct OneServerSocket *good = nullptr, *bad = nullptr;
	GError *last_error = nullptr;

	for (auto &i : ss->sockets) {
		assert(i.serial > 0);
		assert(good == nullptr || i.serial <= good->serial);

		if (bad != nullptr && i.serial != bad->serial) {
			server_socket_close(ss);
			g_propagate_error(error_r, last_error);
			return false;
		}

		GError *error = nullptr;
		if (!i.Open(&error)) {
			if (good != nullptr && good->serial == i.serial) {
				char *address_string = i.ToString();
				char *good_string = good->ToString();
				g_warning("bind to '%s' failed: %s "
					  "(continuing anyway, because "
					  "binding to '%s' succeeded)",
					  address_string, error->message,
					  good_string);
				g_free(address_string);
				g_free(good_string);
				g_error_free(error);
			} else if (bad == nullptr) {
				bad = &i;

				char *address_string = i.ToString();
				g_propagate_prefixed_error(&last_error, error,
							   "Failed to bind to '%s': ",
							   address_string);
				g_free(address_string);
			} else
				g_error_free(error);
			continue;
		}

		/* mark this socket as "good", and clear previous
		   errors */

		good = &i;

		if (bad != nullptr) {
			bad = nullptr;
			g_error_free(last_error);
			last_error = nullptr;
		}
	}

	if (bad != nullptr) {
		server_socket_close(ss);
		g_propagate_error(error_r, last_error);
		return false;
	}

	return true;
}

void
OneServerSocket::Close()
{
	if (fd < 0)
		return;

	g_source_remove(source_id);
	close_socket(fd);
	fd = -1;
}

void
server_socket::Close()
{
	for (auto &i : sockets)
		i.Close();
}

void
server_socket_close(struct server_socket *ss)
{
	ss->Close();
}

static OneServerSocket &
server_socket_add_address(struct server_socket *ss,
			  const struct sockaddr *address,
			  size_t address_length)
{
	assert(ss != nullptr);

	ss->sockets.emplace_front(*ss, ss->next_serial,
				  address, address_length);

	return ss->sockets.front();
}

bool
server_socket_add_fd(struct server_socket *ss, int fd, GError **error_r)
{
	assert(ss != nullptr);
	assert(fd >= 0);

	struct sockaddr_storage address;
	socklen_t address_length;
	if (getsockname(fd, (struct sockaddr *)&address,
			&address_length) < 0) {
		g_set_error(error_r, server_socket_quark(), errno,
			    "Failed to get socket address: %s",
			    g_strerror(errno));
		return false;
	}

	OneServerSocket &s =
		server_socket_add_address(ss, (struct sockaddr *)&address,
					  address_length);
	s.SetFD(fd);

	return true;
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
	struct addrinfo *ai = resolve_host_port(hostname, port,
						AI_PASSIVE, SOCK_STREAM,
						error_r);
	if (ai == nullptr)
		return false;

	for (const struct addrinfo *i = ai; i != nullptr; i = i->ai_next)
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

	OneServerSocket &s =
		server_socket_add_address(ss, (const struct sockaddr *)&s_un,
					  sizeof(s_un));
	s.path = g_strdup(path);

	return true;
#else /* !HAVE_UN */
	(void)ss;
	(void)path;

	g_set_error(error_r, server_socket_quark(), 0,
		    "UNIX domain socket support is disabled");
	return false;
#endif /* !HAVE_UN */
}

