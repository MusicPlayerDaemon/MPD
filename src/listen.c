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

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <netinet/in.h>
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

static int *listenSockets;
static int numberOfListenSockets;
int boundPort;

static gboolean
listen_in_event(GIOChannel *source, GIOCondition condition, gpointer data);

static int establishListen(int pf, const struct sockaddr *addrp,
			   socklen_t addrlen)
{
	int sock;
	int allowReuse = ALLOW_REUSE;
#ifdef HAVE_STRUCT_UCRED
	int passcred = 1;
#endif
	GIOChannel *channel;

	if ((sock = socket(pf, SOCK_STREAM, 0)) < 0)
		g_error("socket < 0");

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&allowReuse,
		       sizeof(allowReuse)) < 0) {
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

	numberOfListenSockets++;
	listenSockets =
	    xrealloc(listenSockets, sizeof(int) * numberOfListenSockets);

	listenSockets[numberOfListenSockets - 1] = sock;

	channel = g_io_channel_unix_new(sock);
	g_io_add_watch(channel, G_IO_IN,
		       listen_in_event, GINT_TO_POINTER(sock));
	g_io_channel_unref(channel);

	return 0;
}

static void
parseListenConfigParam(G_GNUC_UNUSED unsigned int port, ConfigParam * param)
{
	const struct sockaddr *addrp;
	socklen_t addrlen;
#ifdef HAVE_TCP
	struct sockaddr_in sin4;
#ifdef HAVE_IPV6
	struct sockaddr_in6 sin6;
	int useIpv6 = ipv6Supported();

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
		if (useIpv6) {
			sin6.sin6_addr = in6addr_any;
			addrp = (const struct sockaddr *)&sin6;
			addrlen = sizeof(struct sockaddr_in6);
			if (establishListen(PF_INET6, addrp, addrlen) < 0)
				BINDERROR();
		}
#endif
		sin4.sin_addr.s_addr = INADDR_ANY;
		addrp = (const struct sockaddr *)&sin4;
		addrlen = sizeof(struct sockaddr_in);
#ifdef HAVE_IPV6
		if ((establishListen(PF_INET, addrp, addrlen) < 0) && !useIpv6) {
#else
		if (establishListen(PF_INET, addrp, addrlen) < 0) {
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

		if (establishListen(PF_UNIX, addrp, addrlen) < 0)
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

		snprintf(service, sizeof(service), "%u", port);

		ret = getaddrinfo(param->value, service, &hints, &ai);
		if (ret != 0)
			g_error("can't lookup host \"%s\" at line %i: %s",
				param->value, param->line, gai_strerror(ret));

		for (i = ai; i != NULL; i = i->ai_next)
			if (establishListen(i->ai_family, i->ai_addr,
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

		if (establishListen(AF_INET, he->h_addr, he->h_length) < 0)
			BINDERROR();
#endif /* !WIN32 */
#else /* HAVE_TCP */
		g_error("TCP support is disabled");
#endif /* HAVE_TCP */
	}
}

void listenOnPort(void)
{
	int port = DEFAULT_PORT;
	ConfigParam *param = getNextConfigParam(CONF_BIND_TO_ADDRESS, NULL);
	ConfigParam *portParam = getConfigParam(CONF_PORT);

	if (portParam) {
		char *test;
		port = strtol(portParam->value, &test, 10);
		if (port <= 0 || *test != '\0') {
			g_error("%s \"%s\" specified at line %i is not a "
				"positive integer",
				CONF_PORT,
				portParam->value, portParam->line);
		}
	}

	boundPort = port;

	do {
		parseListenConfigParam(port, param);
	} while ((param = getNextConfigParam(CONF_BIND_TO_ADDRESS, param)));
}

void closeAllListenSockets(void)
{
	int i;

	g_debug("closeAllListenSockets called");

	for (i = 0; i < numberOfListenSockets; i++) {
		g_debug("closing listen socket %i", i);
		while (close(listenSockets[i]) < 0 && errno == EINTR) ;
	}

	numberOfListenSockets = 0;
	free(listenSockets);
	listenSockets = NULL;
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
