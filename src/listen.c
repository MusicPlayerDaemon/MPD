/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <resolv.h>
#include <fcntl.h>

#define MAXHOSTNAME 	1024

#define ALLOW_REUSE		1

int listenSocket;

#ifdef HAVE_IPV6
int ipv6Supported() {
	int s;
	s = socket(AF_INET6,SOCK_STREAM,0);
	if(s == -1) return 0;
	close(s);
	return 1;
}
#endif

int establish(unsigned short port) {
	int allowReuse = ALLOW_REUSE;
	int sock;
	struct sockaddr * addrp;
	socklen_t addrlen;
	struct sockaddr_in sin;
	int pf;
#ifdef HAVE_IPV6
	struct sockaddr_in6 sin6;

	memset(&sin6, 0, sizeof(struct sockaddr_in6));
	sin6.sin6_port = htons(port);
	sin6.sin6_family = AF_INET6;
#endif
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;
	
	if(strcmp((getConf())[CONF_BIND_TO_ADDRESS],"any")==0) {
#ifdef HAVE_IPV6
		if(ipv6Supported()) {
			sin6.sin6_addr = in6addr_any;
			addrp = (struct sockaddr *) &sin6;
			addrlen = sizeof(struct sockaddr_in6);
		}
		else 
#endif
		{
			sin.sin_addr.s_addr = INADDR_ANY;
			addrp = (struct sockaddr *) &sin;
			addrlen = sizeof(struct sockaddr_in);
		}
	}
	else {
		struct hostent * he;
		if(!(he = gethostbyname((getConf())[CONF_BIND_TO_ADDRESS]))) {
			ERROR("can't lookup host \"%s\"\n",
					(getConf())[CONF_BIND_TO_ADDRESS]);
			exit(-1);
		}
		switch(he->h_addrtype) {
#ifdef HAVE_IPV6
		case AF_INET6:
			if(!ipv6Supported()) {
				ERROR("no IPv6 support, but a IPv6 address "
					"found for \"%s\"\n",
					(getConf())[CONF_BIND_TO_ADDRESS]);
				exit(-1);
			}
			bcopy((char *)he->h_addr,(char *)
					&sin6.sin6_addr.s6_addr,he->h_length);
			addrp = (struct sockaddr *) &sin6;
			addrlen = sizeof(struct sockaddr_in6);
			break;
#endif
		case AF_INET:
			bcopy((char *)he->h_addr,(char *)&sin.sin_addr.s_addr,
				he->h_length);
			addrp = (struct sockaddr *) &sin;
			addrlen = sizeof(struct sockaddr_in);
			break;
		default:
			ERROR("address type for \"%s\" is not IPv4 or IPv6\n",
					(getConf())[CONF_BIND_TO_ADDRESS]);
			exit(-1);
		}
	}

	switch(addrp->sa_family) {
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
		ERROR("unknown address family: %i\n",addrp->sa_family);
		return -1;
	}

	if((sock = socket(pf,SOCK_STREAM,0)) < 0) {
		ERROR("socket < 0\n");
		return -1;
	}

	if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)&allowReuse,
			sizeof(allowReuse))<0) 
	{
		close(sock);
		ERROR("problems setsockopt'ing\n");
		return -1;
	}

	if(bind(sock,addrp,addrlen)<0) {
		ERROR("unable to bind port %i, maybe MPD is still running?\n",
				port);
		close(sock);
		return -1;
	}
	
	if(listen(sock,5)<0) {
		close(sock);
		ERROR("problems listen'ing\n");
		return -1;
	}

	return sock;
}

void getConnections(int sock) {
	fd_set fdsr;
	int fd = 0;
	struct timeval tv;
	struct sockaddr sockAddr;
	socklen_t socklen = sizeof(sockAddr);
	tv.tv_sec = tv.tv_usec = 0;

	fflush(NULL);
	FD_ZERO(&fdsr);
	FD_SET(sock,&fdsr);

	if(select(sock+1,&fdsr,NULL,NULL,&tv)==1 &&
		((fd = accept(sock,&sockAddr,&socklen)) >= 0)) {
		openAInterface(fd,&sockAddr);
	}
	else if(fd<0) ERROR("Problems accept()'ing\n");
}
