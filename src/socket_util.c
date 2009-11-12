/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "socket_util.h"
#include "fd_util.h"

#include <errno.h>
#include <unistd.h>

#ifndef G_OS_WIN32
#include <sys/socket.h>
#include <netdb.h>
#else /* G_OS_WIN32 */
#include <ws2tcpip.h>
#include <winsock.h>
#endif /* G_OS_WIN32 */

#ifdef HAVE_IPV6
#include <string.h>
#endif

static GQuark
listen_quark(void)
{
	return g_quark_from_static_string("listen");
}

char *
sockaddr_to_string(const struct sockaddr *sa, size_t length, GError **error)
{
#if defined(HAVE_IPV6) && defined(IN6_IS_ADDR_V4MAPPED)
	const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)sa;
	struct sockaddr_in a4;
#endif
	int ret;
	char host[NI_MAXHOST], serv[NI_MAXSERV];

#if defined(HAVE_IPV6) && defined(IN6_IS_ADDR_V4MAPPED)
	if (sa->sa_family == AF_INET6 &&
	    IN6_IS_ADDR_V4MAPPED(&a6->sin6_addr)) {
		/* convert "::ffff:127.0.0.1" to "127.0.0.1" */

		memset(&a4, 0, sizeof(a4));
		a4.sin_family = AF_INET;
		memcpy(&a4.sin_addr, ((const char *)&a6->sin6_addr) + 12,
		       sizeof(a4.sin_addr));
		a4.sin_port = a6->sin6_port;

		sa = (const struct sockaddr *)&a4;
		length = sizeof(a4);
	}
#endif

	ret = getnameinfo(sa, length, host, sizeof(host), serv, sizeof(serv),
			  NI_NUMERICHOST|NI_NUMERICSERV);
	if (ret != 0) {
		g_set_error(error, g_quark_from_static_string("netdb"), ret,
			    "%s", gai_strerror(ret));
		return NULL;
	}

#ifdef HAVE_UN
	if (sa->sa_family == AF_UNIX)
		/* "serv" contains corrupt information with unix
		   sockets */
		return g_strdup(host);
#endif

#ifdef HAVE_IPV6
	if (strchr(host, ':') != NULL)
		return g_strconcat("[", host, "]:", serv, NULL);
#endif

	return g_strconcat(host, ":", serv, NULL);
}

int
socket_bind_listen(int domain, int type, int protocol,
		   const struct sockaddr *address, size_t address_length,
		   int backlog,
		   GError **error)
{
	int fd, ret;
	const int reuse = 1;
#ifdef HAVE_STRUCT_UCRED
	int passcred = 1;
#endif

	fd = socket_cloexec_nonblock(domain, type, protocol);
	if (fd < 0) {
		g_set_error(error, listen_quark(), errno,
			    "Failed to create socket: %s", g_strerror(errno));
		return -1;
	}

	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			 &reuse, sizeof(reuse));
	if (ret < 0) {
		g_set_error(error, listen_quark(), errno,
			    "setsockopt() failed: %s", g_strerror(errno));
		close(fd);
		return -1;
	}

	ret = bind(fd, address, address_length);
	if (ret < 0) {
		g_set_error(error, listen_quark(), errno,
			    "%s", g_strerror(errno));
		close(fd);
		return -1;
	}

	ret = listen(fd, backlog);
	if (ret < 0) {
		g_set_error(error, listen_quark(), errno,
			    "listen() failed: %s", g_strerror(errno));
		close(fd);
		return -1;
	}

#ifdef HAVE_STRUCT_UCRED
	setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &passcred, sizeof(passcred));
#endif

	return fd;
}
