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
#include "interface.h"
#include "conf.h"
#include "log.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <resolv.h>
#include <fcntl.h>

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
static int boundPort;

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

static int establishListen(unsigned int port,
                           struct sockaddr *addrp, socklen_t addrlen)
{
	int pf = 0;
	int sock;
	int allowReuse = ALLOW_REUSE;

	switch (addrp->sa_family) {
	case AF_INET:
		pf = PF_INET;
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		pf = PF_INET6;
		break;
#endif
	case AF_UNIX:
		pf = PF_UNIX;
		break;
	default:
		FATAL("unknown address family: %i\n", addrp->sa_family);
	}

	if ((sock = socket(pf, SOCK_STREAM, 0)) < 0)
		FATAL("socket < 0\n");

	if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0) {
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
	struct sockaddr *addrp = NULL;
	socklen_t addrlen = 0;
	struct sockaddr_in sin;
#ifdef HAVE_IPV6
	struct sockaddr_in6 sin6;
	int useIpv6 = ipv6Supported();

	memset(&sin6, 0, sizeof(struct sockaddr_in6));
	sin6.sin6_port = htons(port);
	sin6.sin6_family = AF_INET6;
#endif
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;

	if (!param || 0 == strcmp(param->value, "any")) {
		DEBUG("binding to any address\n");
#ifdef HAVE_IPV6
		if (useIpv6) {
			sin6.sin6_addr = in6addr_any;
			addrp = (struct sockaddr *)&sin6;
			addrlen = sizeof(struct sockaddr_in6);
			if (establishListen(port, addrp, addrlen) < 0)
				BINDERROR();
		}
#endif
		sin.sin_addr.s_addr = INADDR_ANY;
		addrp = (struct sockaddr *)&sin;
		addrlen = sizeof(struct sockaddr_in);
#ifdef HAVE_IPV6
		if ((establishListen(port, addrp, addrlen) < 0) && !useIpv6) {
#else
		if (establishListen(port, addrp, addrlen) < 0) {
#endif
			BINDERROR();
		}
	} else {
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
			       (char *)he->h_addr, he->h_length);
			addrp = (struct sockaddr *)&sin6;
			addrlen = sizeof(struct sockaddr_in6);
			break;
#endif
		case AF_INET:
			memcpy((char *)&sin.sin_addr.s_addr,
			       (char *)he->h_addr, he->h_length);
			addrp = (struct sockaddr *)&sin;
			addrlen = sizeof(struct sockaddr_in);
			break;
		default:
			FATAL("address type for \"%s\" is not IPv4 or IPv6 "
			      "at line %i\n", param->value, param->line);
		}

		if (establishListen(port, addrp, addrlen) < 0)
			BINDERROR();
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
				openAInterface(fd, &sockAddr);
			} else if (fd < 0
				   && (errno != EAGAIN && errno != EINTR)) {
				ERROR("Problems accept()'ing\n");
			}
		}
	}
}

int getBoundPort(void)
{
	return boundPort;
}
