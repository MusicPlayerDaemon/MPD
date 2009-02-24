/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "listen.h"
#include "client.h"
#include "conf.h"
#include "utils.h"
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

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

#define MAXHOSTNAME 	1024

#define ALLOW_REUSE	1

#define DEFAULT_PORT	6600

#define BINDERROR() do { \
	g_error("unable to bind port %u: %s; " \
		"maybe MPD is still running?", \
		port, strerror(errno)); \
} while (0);

struct listen_socket {
	struct listen_socket *next;

	int fd;

	guint source_id;
};

static struct listen_socket *listen_sockets;
int listen_port;

static gboolean
listen_in_event(GIOChannel *source, GIOCondition condition, gpointer data);

static int
listen_add_address(int pf, const struct sockaddr *addrp, socklen_t addrlen)
{
	int sock;
	const int reuse = ALLOW_REUSE;
#ifdef HAVE_STRUCT_UCRED
	int passcred = 1;
#endif
	struct listen_socket *ls;
	GIOChannel *channel;

	if ((sock = socket(pf, SOCK_STREAM, 0)) < 0)
		g_error("socket < 0");

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		       &reuse, sizeof(reuse)) < 0) {
		g_error("problems setsockopt'ing: %s", strerror(errno));
	}

	if (bind(sock, addrp, addrlen) < 0) {
		close(sock);
		return -1;
	}

	if (listen(sock, 5) < 0)
		g_error("problems listen'ing: %s", strerror(errno));

#ifdef HAVE_STRUCT_UCRED
	setsockopt(sock, SOL_SOCKET, SO_PASSCRED, &passcred, sizeof(passcred));
#endif

	ls = g_new(struct listen_socket, 1);
	ls->fd = sock;

	channel = g_io_channel_unix_new(sock);
	ls->source_id = g_io_add_watch(channel, G_IO_IN,
				       listen_in_event, GINT_TO_POINTER(sock));
	g_io_channel_unref(channel);

	ls->next = listen_sockets;
	listen_sockets = ls;

	return 0;
}

#ifdef HAVE_IPV6
static bool
is_ipv6_enabled(void)
{
	int s;
	s = socket(AF_INET6, SOCK_STREAM, 0);
	if (s == -1)
		return false;
	close(s);
	return true;
}
#endif

static void
listen_add_config_param(G_GNUC_UNUSED unsigned int port,
			const struct config_param *param)
{
	const struct sockaddr *addrp;
	socklen_t addrlen;
#ifdef HAVE_TCP
	struct sockaddr_in sin4;
#ifdef HAVE_IPV6
	struct sockaddr_in6 sin6;
	int ipv6_enabled = is_ipv6_enabled();

	memset(&sin6, 0, sizeof(struct sockaddr_in6));
	sin6.sin6_port = htons(port);
	sin6.sin6_family = AF_INET6;
#endif
	memset(&sin4, 0, sizeof(struct sockaddr_in));
	sin4.sin_port = htons(port);
	sin4.sin_family = AF_INET;
#endif /* HAVE_TCP */

	if (!param || 0 == strcmp(param->value, "any")) {
#ifdef HAVE_TCP
		g_debug("binding to any address");
#ifdef HAVE_IPV6
		if (ipv6_enabled) {
			sin6.sin6_addr = in6addr_any;
			addrp = (const struct sockaddr *)&sin6;
			addrlen = sizeof(struct sockaddr_in6);
			if (listen_add_address(PF_INET6, addrp, addrlen) < 0)
				BINDERROR();
		}
#endif
		sin4.sin_addr.s_addr = INADDR_ANY;
		addrp = (const struct sockaddr *)&sin4;
		addrlen = sizeof(struct sockaddr_in);
#ifdef HAVE_IPV6
		if ((listen_add_address(PF_INET, addrp, addrlen) < 0) && !ipv6_enabled) {
#else
		if (listen_add_address(PF_INET, addrp, addrlen) < 0) {
#endif
			BINDERROR();
		}
#else /* HAVE_TCP */
		g_error("TCP support is disabled");
#endif /* HAVE_TCP */
#ifdef HAVE_UN
	} else if (param->value[0] == '/') {
		size_t path_length;
		struct sockaddr_un s_un;

		path_length = strlen(param->value);
		if (path_length >= sizeof(s_un.sun_path))
			g_error("unix socket path is too long");

		unlink(param->value);

		s_un.sun_family = AF_UNIX;
		memcpy(s_un.sun_path, param->value, path_length + 1);

		addrp = (const struct sockaddr *)&s_un;
		addrlen = sizeof(s_un);

		if (listen_add_address(PF_UNIX, addrp, addrlen) < 0)
			g_error("unable to bind to %s: %s",
				param->value, strerror(errno));

		/* allow everybody to connect */
		chmod(param->value, 0666);

#endif /* HAVE_UN */
	} else {
#ifdef HAVE_TCP
#ifndef WIN32
		struct addrinfo hints, *ai, *i;
		char service[20];
		int ret;

		g_debug("binding to address for %s", param->value);

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_PASSIVE;
#ifdef AI_ADDRCONFIG
		hints.ai_flags |= AI_ADDRCONFIG;
#endif
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		g_snprintf(service, sizeof(service), "%u", port);

		ret = getaddrinfo(param->value, service, &hints, &ai);
		if (ret != 0)
			g_error("can't lookup host \"%s\" at line %i: %s",
				param->value, param->line, gai_strerror(ret));

		for (i = ai; i != NULL; i = i->ai_next)
			if (listen_add_address(i->ai_family, i->ai_addr,
					       i->ai_addrlen) < 0)
				BINDERROR();

		freeaddrinfo(ai);
#else /* WIN32 */
		const struct hostent *he;

		g_debug("binding to address for %s", param->value);

		he = gethostbyname(param->value);
		if (he == NULL)
			g_error("can't lookup host \"%s\" at line %i",
				param->value, param->line);

		if (he->h_addrtype != AF_INET)
			g_error("IPv4 address expected for host \"%s\" at line %i",
				param->value, param->line);

		if (listen_add_address(AF_INET, he->h_addr, he->h_length) < 0)
			BINDERROR();
#endif /* !WIN32 */
#else /* HAVE_TCP */
		g_error("TCP support is disabled");
#endif /* HAVE_TCP */
	}
}

void listen_global_init(void)
{
	int port = config_get_positive(CONF_PORT, DEFAULT_PORT);
	const struct config_param *param =
		config_get_next_param(CONF_BIND_TO_ADDRESS, NULL);

	do {
		listen_add_config_param(port, param);
	} while ((param = config_get_next_param(CONF_BIND_TO_ADDRESS, param)));
	listen_port = port;
}

void listen_global_finish(void)
{
	g_debug("listen_global_finish called");

	while (listen_sockets != NULL) {
		struct listen_socket *ls = listen_sockets;
		listen_sockets = ls->next;

		g_source_remove(ls->source_id);
		close(ls->fd);
		g_free(ls);
	}
}

static int get_remote_uid(int fd)
{
#ifdef HAVE_STRUCT_UCRED
	struct ucred cred;
	socklen_t len = sizeof (cred);

	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0)
		return 0;

	return cred.uid;
#else
	(void)fd;
	return -1;
#endif
}

static gboolean
listen_in_event(G_GNUC_UNUSED GIOChannel *source,
		G_GNUC_UNUSED GIOCondition condition,
		gpointer data)
{
	int listen_fd = GPOINTER_TO_INT(data), fd;
	struct sockaddr sockAddr;
	socklen_t socklen = sizeof(sockAddr);

	fd = accept(listen_fd, &sockAddr, &socklen);
	if (fd >= 0) {
		set_nonblocking(fd);

		client_new(fd, &sockAddr, get_remote_uid(fd));
	} else if (fd < 0 && errno != EINTR) {
		g_warning("Problems accept()'ing");
	}

	return true;
}
