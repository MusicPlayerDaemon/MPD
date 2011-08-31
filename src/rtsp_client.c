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

/*
 * Based on the RTSP client by Shiro Ninomiya <shiron@snino.com>
 */

#include "rtsp_client.h"
#include "glib_compat.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#ifdef WIN32
#define WINVER 0x0501
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <netdb.h>
#endif

/*
 * Free all memory associated with key_data
 */
void
free_kd(struct key_data *kd)
{
	struct key_data *iter = kd;
	while (iter) {
		g_free(iter->key);
		g_free(iter->data);
		iter = iter->next;
		g_free(kd);
		kd = iter;
	}
}

/*
 * key_data type data look up
 */
char *
kd_lookup(struct key_data *kd, const char *key)
{
	while (kd) {
		if (!strcmp(kd->key, key)) {
			return kd->data;
		}
		kd = kd->next;
	}
	return NULL;
}

struct rtspcl_data *
rtspcl_open(void)
{
	struct rtspcl_data *rtspcld;
	rtspcld = g_new0(struct rtspcl_data, 1);
	rtspcld->useragent = "RTSPClient";
	return rtspcld;
}

/* bind an opened socket to specified hostname and port.
 * if hostname=NULL, use INADDR_ANY.
 * if *port=0, use dynamically assigned port
 */
static int bind_host(int sd, char *hostname, unsigned long ulAddr,
		     unsigned short *port, GError **error_r)
{
	struct sockaddr_in my_addr;
	socklen_t nlen = sizeof(struct sockaddr);
	struct hostent *h;

	memset(&my_addr, 0, sizeof(my_addr));
	/* use specified hostname */
	if (hostname) {
		/* get server IP address (no check if input is IP address or DNS name) */
		h = gethostbyname(hostname);
		if (h == NULL) {
			if (strstr(hostname, "255.255.255.255") == hostname) {
				my_addr.sin_addr.s_addr=-1;
			} else {
				if ((my_addr.sin_addr.s_addr = inet_addr(hostname)) == 0xFFFFFFFF) {
					g_set_error(error_r, rtsp_client_quark(), 0,
						    "failed to resolve host '%s'",
						    hostname);
					return -1;
				}
			}
			my_addr.sin_family = AF_INET;
		} else {
			my_addr.sin_family = h->h_addrtype;
			memcpy((char *) &my_addr.sin_addr.s_addr,
			       h->h_addr_list[0], h->h_length);
		}
	} else {
		// if hostname=NULL, use INADDR_ANY
		if (ulAddr)
			my_addr.sin_addr.s_addr = ulAddr;
		else
			my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		my_addr.sin_family = AF_INET;
	}

	/* bind a specified port */
	my_addr.sin_port = htons(*port);

	if (bind(sd, (struct sockaddr *) &my_addr, sizeof(my_addr)) < 0) {
		g_set_error(error_r, rtsp_client_quark(), errno,
			    "failed to bind socket: %s",
			    g_strerror(errno));
		return -1;
	}

	if (*port == 0) {
		getsockname(sd, (struct sockaddr *) &my_addr, &nlen);
		*port = ntohs(my_addr.sin_port);
	}

	return 0;
}

/*
 * open tcp port
 */
static int
open_tcp_socket(char *hostname, unsigned short *port,
		GError **error_r)
{
	int sd;

	/* socket creation */
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd < 0) {
		g_set_error(error_r, rtsp_client_quark(), errno,
			    "failed to create TCP socket: %s",
			    g_strerror(errno));
		return -1;
	}
	if (bind_host(sd, hostname, 0, port, error_r)) {
		close(sd);
		return -1;
	}

	return sd;
}

static bool
get_sockaddr_by_host(const char *host, short destport,
		     struct sockaddr_in *addr,
		     GError **error_r)
{
	struct hostent *h;

	h = gethostbyname(host);
	if (h) {
		addr->sin_family = h->h_addrtype;
		memcpy((char *) &addr->sin_addr.s_addr, h->h_addr_list[0], h->h_length);
	} else {
		addr->sin_family = AF_INET;
		if ((addr->sin_addr.s_addr=inet_addr(host))==0xFFFFFFFF) {
			g_set_error(error_r, rtsp_client_quark(), 0,
				    "failed to resolve host '%s'", host);
			return false;
		}
	}
	addr->sin_port = htons(destport);
	return true;
}

/*
 * create tcp connection
 * as long as the socket is not non-blocking, this can block the process
 * nsport is network byte order
 */
static bool
get_tcp_connect(int sd, struct sockaddr_in dest_addr, GError **error_r)
{
	if (connect(sd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr))){
		g_usleep(100000);
		// try one more time
		if (connect(sd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr))) {
			g_set_error(error_r, rtsp_client_quark(), errno,
				    "failed to connect to %s:%d: %s",
				    inet_ntoa(dest_addr.sin_addr),
				    ntohs(dest_addr.sin_port),
				    g_strerror(errno));
			return false;
		}
	}
	return true;
}

static bool
get_tcp_connect_by_host(int sd, const char *host, short destport,
			GError **error_r)
{
	struct sockaddr_in addr;

	return get_sockaddr_by_host(host, destport, &addr, error_r) &&
		get_tcp_connect(sd, addr, error_r);
}

bool
rtspcl_connect(struct rtspcl_data *rtspcld, const char *host, short destport,
	       const char *sid, GError **error_r)
{
	unsigned short myport = 0;
	struct sockaddr_in name;
	socklen_t namelen = sizeof(name);

	if ((rtspcld->fd = open_tcp_socket(NULL, &myport, error_r)) == -1)
		return false;

	if (!get_tcp_connect_by_host(rtspcld->fd, host, destport, error_r))
		return false;

	getsockname(rtspcld->fd, (struct sockaddr*)&name, &namelen);
	memcpy(&rtspcld->local_addr, &name.sin_addr,sizeof(struct in_addr));
	sprintf(rtspcld->url, "rtsp://%s/%s", inet_ntoa(name.sin_addr), sid);
	getpeername(rtspcld->fd, (struct sockaddr*)&name, &namelen);
	memcpy(&rtspcld->host_addr, &name.sin_addr, sizeof(struct in_addr));
	return true;
}

static void
rtspcl_disconnect(struct rtspcl_data *rtspcld)
{
	if (rtspcld->fd > 0) close(rtspcld->fd);
	rtspcld->fd = 0;
}

static void
rtspcl_remove_all_exthds(struct rtspcl_data *rtspcld)
{
	free_kd(rtspcld->exthds);
	rtspcld->exthds = NULL;
}

void
rtspcl_close(struct rtspcl_data *rtspcld)
{
	rtspcl_disconnect(rtspcld);
	rtspcl_remove_all_exthds(rtspcld);
	g_free(rtspcld->session);
	g_free(rtspcld);
}

void
rtspcl_add_exthds(struct rtspcl_data *rtspcld, const char *key, char *data)
{
	struct key_data *new_kd;
	new_kd = g_new(struct key_data, 1);
	new_kd->key = g_strdup(key);
	new_kd->data = g_strdup(data);
	new_kd->next = NULL;
	if (!rtspcld->exthds) {
		rtspcld->exthds = new_kd;
	} else {
		struct key_data *iter = rtspcld->exthds;
		while (iter->next) {
			iter = iter->next;
		}
		iter->next = new_kd;
	}
}

/*
 * read one line from the file descriptor
 * timeout: msec unit, -1 for infinite
 * if CR comes then following LF is expected
 * returned string in line is always null terminated, maxlen-1 is maximum string length
 */
static int
read_line(int fd, char *line, int maxlen, int timeout, int no_poll)
{
	int i, rval;
	int count = 0;
	struct pollfd pfds;
	char ch;
	*line = 0;
	pfds.events = POLLIN;
	pfds.fd = fd;
	for (i = 0;i < maxlen; i++) {
		if (no_poll || poll(&pfds, 1, timeout))
			rval=read(fd,&ch,1);
		else return 0;

		if (rval == -1) {
			if (errno == EAGAIN) return 0;
			g_warning("%s:read error: %s\n", __func__, strerror(errno));
			return -1;
		}
		if (rval == 0) {
			g_debug("%s:disconnected on the other end\n", __func__);
			return -1;
		}
		if(ch == '\n') {
			*line = 0;
			return count;
		}
		if (ch == '\r') continue;
		*line++ = ch;
		count++;
		if (count >= maxlen - 1) break;
	}
	*line = 0;
	return count;
}

/*
 * send RTSP request, and get response if it's needed
 * if this gets a success, *kd is allocated or reallocated (if *kd is not NULL)
 */
bool
exec_request(struct rtspcl_data *rtspcld, const char *cmd,
	     const char *content_type, const char *content,
	     int get_response,
	     const struct key_data *hds, struct key_data **kd,
	     GError **error_r)
{
	char line[1024];
	char req[1024];
	char reql[128];
	const char delimiters[] = " ";
	char *token, *dp;
	int dsize = 0,rval;
	int timeout = 5000; // msec unit

	fd_set rdfds;
	int fdmax = 0;
	struct timeval tout = {.tv_sec=10, .tv_usec=0};

	if (!rtspcld) {
		g_set_error_literal(error_r, rtsp_client_quark(), 0,
				    "not connected");
		return false;
	}

	sprintf(req, "%s %s RTSP/1.0\r\nCSeq: %d\r\n", cmd, rtspcld->url, ++rtspcld->cseq );

	if ( rtspcld->session != NULL ) {
		sprintf(reql,"Session: %s\r\n", rtspcld->session );
		strncat(req,reql,sizeof(req));
	}

	const struct key_data *hd_iter = hds;
	while (hd_iter) {
		sprintf(reql, "%s: %s\r\n", hd_iter->key, hd_iter->data);
		strncat(req, reql, sizeof(req));
		hd_iter = hd_iter->next;
	}

	if (content_type && content) {
		sprintf(reql, "Content-Type: %s\r\nContent-Length: %d\r\n",
			content_type, (int) strlen(content));
		strncat(req,reql,sizeof(req));
	}

	sprintf(reql, "User-Agent: %s\r\n", rtspcld->useragent);
	strncat(req, reql, sizeof(req));

	hd_iter = rtspcld->exthds;
	while (hd_iter) {
		sprintf(reql, "%s: %s\r\n", hd_iter->key, hd_iter->data);
		strncat(req, reql, sizeof(req));
		hd_iter = hd_iter->next;
	}
	strncat(req, "\r\n", sizeof(req));

	if (content_type && content)
		strncat(req, content, sizeof(req));

	rval = write(rtspcld->fd, req, strlen(req));
	if (rval < 0) {
		g_set_error(error_r, rtsp_client_quark(), errno,
			    "write error: %s",
			    g_strerror(errno));
		return false;
	}

	if (!get_response) return true;

	while (true) {
		FD_ZERO(&rdfds);
		FD_SET(rtspcld->fd, &rdfds);
		fdmax = rtspcld->fd;
		select(fdmax + 1, &rdfds, NULL, NULL, &tout);
		if (FD_ISSET(rtspcld->fd, &rdfds)) {
			break;
		}
	}

	if (read_line(rtspcld->fd, line, sizeof(line), timeout, 0) <= 0) {
		g_set_error_literal(error_r, rtsp_client_quark(), 0,
				    "request failed");
		return false;
	}

	token = strtok(line, delimiters);
	token = strtok(NULL, delimiters);
	if (token == NULL) {
		g_set_error_literal(error_r, rtsp_client_quark(), 0,
				    "request failed");
		return false;
	}

	if (strcmp(token, "200") != 0) {
		g_set_error(error_r, rtsp_client_quark(), 0,
			    "request failed: %s", token);
		return false;
	}

	/* if the caller isn't interested in response headers, put
	   them on the trash, which is freed before returning from
	   this function */
	struct key_data *trash = NULL;
	if (kd == NULL)
		kd = &trash;

	struct key_data *cur_kd = *kd;

	struct key_data *new_kd = NULL;
	while (read_line(rtspcld->fd, line, sizeof(line), timeout, 0) > 0) {
		timeout = 1000; // once it started, it shouldn't take a long time
		if (new_kd != NULL && line[0] == ' ') {
			const char *j = line;
			while (*j == ' ')
				++j;

			dsize += strlen(j);
			new_kd->data = g_realloc(new_kd->data, dsize);
			strcat(new_kd->data, j);
			continue;
		}
		dp = strstr(line, ":");
		if (!dp) {
			free_kd(*kd);
			*kd = NULL;

			g_set_error_literal(error_r, rtsp_client_quark(), 0,
					    "request failed, bad header");
			return false;
		}

		*dp++ = 0;
		new_kd = g_new(struct key_data, 1);
		new_kd->key = g_strdup(line);
		dsize = strlen(dp) + 1;
		new_kd->data = g_strdup(dp);
		new_kd->next = NULL;
		if (cur_kd == NULL) {
			cur_kd = *kd = new_kd;
		} else {
			cur_kd->next = new_kd;
			cur_kd = new_kd;
		}
	}

	free_kd(trash);

	return true;
}

bool
rtspcl_set_parameter(struct rtspcl_data *rtspcld, const char *parameter,
		     GError **error_r)
{
	return exec_request(rtspcld, "SET_PARAMETER", "text/parameters",
			    parameter, 1, NULL, NULL, error_r);
}

void
rtspcl_set_useragent(struct rtspcl_data *rtspcld, const char *name)
{
	rtspcld->useragent = name;
}

bool
rtspcl_announce_sdp(struct rtspcl_data *rtspcld, const char *sdp,
		    GError **error_r)
{
	return exec_request(rtspcld, "ANNOUNCE", "application/sdp", sdp, 1,
			    NULL, NULL, error_r);
}

bool
rtspcl_setup(struct rtspcl_data *rtspcld, struct key_data **kd,
	     int control_port, int ntp_port,
	     GError **error_r)
{
	struct key_data *rkd = NULL, hds;
	const char delimiters[] = ";";
	char *buf = NULL;
	char *token, *pc;
	int rval = false;

	static char transport_key[] = "Transport";

	char transport_value[256];
	snprintf(transport_value, sizeof(transport_value),
		 "RTP/AVP/UDP;unicast;interleaved=0-1;mode=record;control_port=%d;timing_port=%d",
		 control_port, ntp_port);

	hds.key = transport_key;
	hds.data = transport_value;
	hds.next = NULL;
	if (!exec_request(rtspcld, "SETUP", NULL, NULL, 1,
			  &hds, &rkd, error_r))
		return false;

	if (!(rtspcld->session = g_strdup(kd_lookup(rkd, "Session")))) {
		g_set_error_literal(error_r, rtsp_client_quark(), 0,
				    "no session in response");
		goto erexit;
	}
	if (!(rtspcld->transport = kd_lookup(rkd, "Transport"))) {
		g_set_error_literal(error_r, rtsp_client_quark(), 0,
				    "no transport in response");
		goto erexit;
	}
	buf = g_strdup(rtspcld->transport);
	token = strtok(buf, delimiters);
	rtspcld->server_port = 0;
	rtspcld->control_port = 0;
	while (token) {
		if ((pc = strstr(token, "="))) {
			*pc = 0;
			if (!strcmp(token,"server_port")) {
				rtspcld->server_port=atoi(pc + 1);
			}
			if (!strcmp(token,"control_port")) {
				rtspcld->control_port=atoi(pc + 1);
			}
		}
		token = strtok(NULL, delimiters);
	}
	if (rtspcld->server_port == 0) {
		g_set_error_literal(error_r, rtsp_client_quark(), 0,
				    "no server_port in response");
		goto erexit;
	}
	if (rtspcld->control_port == 0) {
		g_set_error_literal(error_r, rtsp_client_quark(), 0,
				    "no control_port in response");
		goto erexit;
	}
	rval = true;
 erexit:
	g_free(buf);
	if (!rval) {
		free_kd(rkd);
		rkd = NULL;
	}
	*kd = rkd;
	return rval;
}

bool
rtspcl_record(struct rtspcl_data *rtspcld,
	      int seq_num, int rtptime,
	      GError **error_r)
{
	if (!rtspcld->session) {
		g_set_error_literal(error_r, rtsp_client_quark(), 0,
				    "no session in progress");
		return false;
	}

	char buf[128];
	sprintf(buf, "seq=%d,rtptime=%u", seq_num, rtptime);

	struct key_data rtp;
	static char rtp_key[] = "RTP-Info";
	rtp.key = rtp_key;
	rtp.data = buf;
	rtp.next = NULL;

	struct key_data range;
	static char range_key[] = "Range";
	range.key = range_key;
	static char range_value[] = "npt=0-";
	range.data = range_value;
	range.next = &rtp;

	return exec_request(rtspcld, "RECORD", NULL, NULL, 1, &range,
			    NULL, error_r);
}

char *
rtspcl_local_ip(struct rtspcl_data *rtspcld)
{
	return inet_ntoa(rtspcld->local_addr);
}
