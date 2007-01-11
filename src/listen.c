/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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
	ERROR("unable to bind port %u: %s\n", port, strerror(errno)); \
	ERROR("maybe MPD is still running?\n"); \
} while (0);

int *listenSockets = NULL;
int numberOfListenSockets = 0;
static int boundPort = 0;

static int establishListen(unsigned int port,
                           struct sockaddr *addrp, socklen_t addrlen)
{
	int pf;
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
		ERROR("unknown address family: %i\n", addrp->sa_family);
		exit(EXIT_FAILURE);
	}

	if ((sock = socket(pf, SOCK_STREAM, 0)) < 0) {
		ERROR("socket < 0\n");
		exit(EXIT_FAILURE);
	}

	if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0) {
		ERROR("problems setting nonblocking on listen socket: %s\n",
		      strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&allowReuse,
		       sizeof(allowReuse)) < 0) {
		ERROR("problems setsockopt'ing: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (bind(sock, addrp, addrlen) < 0) {
		close(sock);
		return -1;
	}

	if (listen(sock, 5) < 0) {
		ERROR("problems listen'ing: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	numberOfListenSockets++;
	listenSockets =
	    xrealloc(listenSockets, sizeof(int) * numberOfListenSockets);

	listenSockets[numberOfListenSockets - 1] = sock;

	return 0;
}

static void parseListenConfigParam(unsigned int port, ConfigParam * param)
{
	struct sockaddr *addrp;
	socklen_t addrlen;
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
			if (establishListen(port, addrp, addrlen) < 0) {
				BINDERROR();
				exit(EXIT_FAILURE);
			}
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
			exit(EXIT_FAILURE);
		}
	} else {
		struct hostent *he;
		DEBUG("binding to address for %s\n", param->value);
		if (!(he = gethostbyname(param->value))) {
			ERROR("can't lookup host \"%s\" at line %i\n",
			      param->value, param->line);
			exit(EXIT_FAILURE);
		}
		switch (he->h_addrtype) {
#ifdef HAVE_IPV6
		case AF_INET6:
			if (!useIpv6) {
				ERROR("no IPv6 support, but a IPv6 address "
				      "found for \"%s\" at line %i\n",
				      param->value, param->line);
				exit(EXIT_FAILURE);
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
			ERROR("address type for \"%s\" is not IPv4 or IPv6 "
			      "at line %i\n", param->value, param->line);
			exit(EXIT_FAILURE);
		}

		if (establishListen(port, addrp, addrlen) < 0) {
			BINDERROR();
			exit(EXIT_FAILURE);
		}
	}
}

void listenOnPort(void)
{
	int port = DEFAULT_PORT;
	ConfigParam *param = getNextConfigParam(CONF_BIND_TO_ADDRESS, NULL);

	{
		ConfigParam *portParam = getConfigParam(CONF_PORT);

		if (portParam) {
			char *test;
			port = strtol(portParam->value, &test, 10);
			if (port <= 0 || *test != '\0') {
				ERROR("%s \"%s\" specified at line %i is not a "
				      "positive integer", CONF_PORT,
				      portParam->value, portParam->line);
				exit(EXIT_FAILURE);
			}
		}
	}

	boundPort = port;

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

int getBoundPort()
{
	return boundPort;
}
