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
#include "log.h"
#include "utils.h"
#include "os_compat.h"

#include "../config.h"

#define MAXHOSTNAME 	1024

#define ALLOW_REUSE	1

#define DEFAULT_PORT	6600

#define BINDERROR() do { \
	FATAL("unable to bind port %u: %s\n" \
	      "maybe MPD is still running?\n", \
	      port, strerror(errno)); \
} while (0);

static int *listenSockets;
static int numberOfListenSockets;
int boundPort;

/*
 * redirect stdin to /dev/null to work around a libao bug
 * there are likely other bugs in other libraries (and even our code!)
 * that check for fd > 0, so it's easiest to just keep
 * fd = 0 == /dev/null for now...
 */
static void redirect_stdin(void)
{
	int fd, st;
	struct stat ss;

	if ((st = fstat(STDIN_FILENO, &ss)) < 0) {
		if ((fd = open("/dev/null", O_RDONLY) > 0)) {
			DEBUG("stdin closed, and could not open /dev/null "
			      "as fd=0, some external library bugs "
			      "may be exposed...\n");
			close(fd);
		}
		return;
	}
	if (!isatty(STDIN_FILENO))
		return;
	if ((fd = open("/dev/null", O_RDONLY)) < 0)
		FATAL("failed to open /dev/null %s\n", strerror(errno));
	if (dup2(fd, STDIN_FILENO) < 0)
		FATAL("dup2 stdin: %s\n", strerror(errno));
}

static int establishListen(const struct sockaddr *addrp, socklen_t addrlen)
{
	int pf;
	int sock;
	int allowReuse = ALLOW_REUSE;

	switch (addrp->sa_family) {
#ifdef HAVE_TCP
	case AF_INET:
		pf = PF_INET;
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		pf = PF_INET6;
		break;
#endif
#endif /* HAVE_TCP */
#ifdef HAVE_UN
	case AF_UNIX:
		pf = PF_UNIX;
		break;
#endif /* HAVE_UN */
	default:
		FATAL("unknown address family: %i\n", addrp->sa_family);
	}

	if ((sock = socket(pf, SOCK_STREAM, 0)) < 0)
		FATAL("socket < 0\n");

	if (set_nonblocking(sock) < 0) {
		FATAL("problems setting nonblocking on listen socket: %s\n",
		      strerror(errno));
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&allowReuse,
		       sizeof(allowReuse)) < 0) {
		FATAL("problems setsockopt'ing: %s\n", strerror(errno));
	}

	if (bind(sock, addrp, addrlen) < 0) {
		close(sock);
		return -1;
	}

	if (listen(sock, 5) < 0)
		FATAL("problems listen'ing: %s\n", strerror(errno));

	numberOfListenSockets++;
	listenSockets =
	    xrealloc(listenSockets, sizeof(int) * numberOfListenSockets);

	listenSockets[numberOfListenSockets - 1] = sock;

	return 0;
}

static void parseListenConfigParam(unsigned int port, ConfigParam * param)
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
		DEBUG("binding to any address\n");
#ifdef HAVE_IPV6
		if (useIpv6) {
			sin6.sin6_addr = in6addr_any;
			addrp = (const struct sockaddr *)&sin6;
			addrlen = sizeof(struct sockaddr_in6);
			if (establishListen(addrp, addrlen) < 0)
				BINDERROR();
		}
#endif
		sin4.sin_addr.s_addr = INADDR_ANY;
		addrp = (const struct sockaddr *)&sin4;
		addrlen = sizeof(struct sockaddr_in);
#ifdef HAVE_IPV6
		if ((establishListen(addrp, addrlen) < 0) && !useIpv6) {
#else
		if (establishListen(addrp, addrlen) < 0) {
#endif
			BINDERROR();
		}
#else /* HAVE_TCP */
		FATAL("TCP support is disabled\n");
#endif /* HAVE_TCP */
#ifdef HAVE_UN
	} else if (param->value[0] == '/') {
		size_t path_length;
		struct sockaddr_un sun;

		path_length = strlen(param->value);
		if (path_length >= sizeof(sun.sun_path))
			FATAL("unix socket path is too long\n");

		unlink(param->value);

		sun.sun_family = AF_UNIX;
		memcpy(sun.sun_path, param->value, path_length + 1);

		addrp = (const struct sockaddr *)&sun;
		addrlen = sizeof(sun);

		if (establishListen(addrp, addrlen) < 0)
			FATAL("unable to bind to %s: %s\n",
			      param->value, strerror(errno));
#endif /* HAVE_UN */
	} else {
#ifdef HAVE_TCP
		struct hostent *he;
		DEBUG("binding to address for %s\n", param->value);
		if (!(he = gethostbyname(param->value))) {
			FATAL("can't lookup host \"%s\" at line %i\n",
			      param->value, param->line);
		}
		switch (he->h_addrtype) {
#ifdef HAVE_IPV6
		case AF_INET6:
			if (!useIpv6) {
				FATAL("no IPv6 support, but a IPv6 address "
				      "found for \"%s\" at line %i\n",
				      param->value, param->line);
			}
			memcpy((char *)&sin6.sin6_addr.s6_addr,
			       (const char *)he->h_addr, he->h_length);
			addrp = (const struct sockaddr *)&sin6;
			addrlen = sizeof(struct sockaddr_in6);
			break;
#endif
		case AF_INET:
			memcpy((char *)&sin4.sin_addr.s_addr,
			       (const char *)he->h_addr, he->h_length);
			addrp = (struct sockaddr *)&sin4;
			addrlen = sizeof(struct sockaddr_in);
			break;
		default:
			FATAL("address type for \"%s\" is not IPv4 or IPv6 "
			      "at line %i\n", param->value, param->line);
		}

		if (establishListen(addrp, addrlen) < 0)
			BINDERROR();
#else /* HAVE_TCP */
		FATAL("TCP support is disabled\n");
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
			FATAL("%s \"%s\" specified at line %i is not a "
			      "positive integer", CONF_PORT,
			      portParam->value, portParam->line);
		}
	}

	boundPort = port;

	redirect_stdin();
	do {
		parseListenConfigParam(port, param);
	} while ((param = getNextConfigParam(CONF_BIND_TO_ADDRESS, param)));
}

void addListenSocketsToFdSet(fd_set * fds, int *fdmax)
{
	int i;

	for (i = 0; i < numberOfListenSockets; i++) {
		FD_SET(listenSockets[i], fds);
		if (listenSockets[i] > *fdmax)
			*fdmax = listenSockets[i];
	}
}

void closeAllListenSockets(void)
{
	int i;

	DEBUG("closeAllListenSockets called\n");

	for (i = 0; i < numberOfListenSockets; i++) {
		DEBUG("closing listen socket %i\n", i);
		while (close(listenSockets[i]) < 0 && errno == EINTR) ;
	}
	freeAllListenSockets();
}

void freeAllListenSockets(void)
{
	numberOfListenSockets = 0;
	free(listenSockets);
	listenSockets = NULL;
}

void getConnections(fd_set * fds)
{
	int i;
	int fd = 0;
	struct sockaddr sockAddr;
	socklen_t socklen = sizeof(sockAddr);

	for (i = 0; i < numberOfListenSockets; i++) {
		if (FD_ISSET(listenSockets[i], fds)) {
			if ((fd = accept(listenSockets[i], &sockAddr, &socklen))
			    >= 0) {
				client_new(fd, &sockAddr);
			} else if (fd < 0
				   && (errno != EAGAIN && errno != EINTR)) {
				ERROR("Problems accept()'ing\n");
			}
		}
	}
}
