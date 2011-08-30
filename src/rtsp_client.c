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
#include "tcp_socket.h"
#include "glib_compat.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef WIN32
#define WINVER 0x0501
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
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
	rtspcld->mutex = g_mutex_new();
	rtspcld->cond = g_cond_new();
	rtspcld->received_lines = g_queue_new();
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

static void
rtsp_client_flush_received(struct rtspcl_data *rtspcld)
{
	char *line;
	while ((line = g_queue_pop_head(rtspcld->received_lines)) != NULL)
		g_free(line);
}

static size_t
rtsp_client_socket_data(const void *_data, size_t length, void *ctx)
{
	struct rtspcl_data *rtspcld = ctx;

	g_mutex_lock(rtspcld->mutex);

	if (rtspcld->tcp_socket == NULL) {
		g_mutex_unlock(rtspcld->mutex);
		return 0;
	}

	const bool was_empty = g_queue_is_empty(rtspcld->received_lines);
	bool added = false;
	const char *data = _data, *end = data + length, *p = data, *eol;
	while ((eol = memchr(p, '\n', end - p)) != NULL) {
		const char *next = eol + 1;

		if (rtspcld->received_lines->length < 64) {
			if (eol > p && eol[-1] == '\r')
				--eol;

			g_queue_push_tail(rtspcld->received_lines,
					  g_strndup(p, eol - p));
			added = true;
		}

		p = next;
	}

	if (was_empty && added)
		g_cond_broadcast(rtspcld->cond);

	g_mutex_unlock(rtspcld->mutex);

	return p - data;
}

static void
rtsp_client_socket_error(GError *error, void *ctx)
{
	struct rtspcl_data *rtspcld = ctx;

	g_warning("%s", error->message);
	g_error_free(error);

	g_mutex_lock(rtspcld->mutex);

	rtsp_client_flush_received(rtspcld);

	struct tcp_socket *s = rtspcld->tcp_socket;
	rtspcld->tcp_socket = NULL;

	g_cond_broadcast(rtspcld->cond);

	g_mutex_unlock(rtspcld->mutex);

	if (s != NULL)
		tcp_socket_free(s);
}

static void
rtsp_client_socket_disconnected(void *ctx)
{
	struct rtspcl_data *rtspcld = ctx;

	g_mutex_lock(rtspcld->mutex);

	rtsp_client_flush_received(rtspcld);

	struct tcp_socket *s = rtspcld->tcp_socket;
	rtspcld->tcp_socket = NULL;

	g_cond_broadcast(rtspcld->cond);

	g_mutex_unlock(rtspcld->mutex);

	if (s != NULL)
		tcp_socket_free(s);
}

static const struct tcp_socket_handler rtsp_client_socket_handler = {
	.data = rtsp_client_socket_data,
	.error = rtsp_client_socket_error,
	.disconnected = rtsp_client_socket_disconnected,
};

bool
rtspcl_connect(struct rtspcl_data *rtspcld, const char *host, short destport,
	       const char *sid, GError **error_r)
{
	assert(rtspcld->tcp_socket == NULL);

	unsigned short myport = 0;
	struct sockaddr_in name;
	socklen_t namelen = sizeof(name);

	int fd = open_tcp_socket(NULL, &myport, error_r);
	if (fd < 0)
		return false;

	if (!get_tcp_connect_by_host(fd, host, destport, error_r))
		return false;

	getsockname(fd, (struct sockaddr*)&name, &namelen);
	memcpy(&rtspcld->local_addr, &name.sin_addr,sizeof(struct in_addr));
	sprintf(rtspcld->url, "rtsp://%s/%s", inet_ntoa(name.sin_addr), sid);
	getpeername(fd, (struct sockaddr*)&name, &namelen);
	memcpy(&rtspcld->host_addr, &name.sin_addr, sizeof(struct in_addr));

	rtspcld->tcp_socket = tcp_socket_new(fd, &rtsp_client_socket_handler,
					     rtspcld);

	return true;
}

static void
rtspcl_disconnect(struct rtspcl_data *rtspcld)
{
	g_mutex_lock(rtspcld->mutex);
	rtsp_client_flush_received(rtspcld);
	g_mutex_unlock(rtspcld->mutex);

	if (rtspcld->tcp_socket != NULL) {
		tcp_socket_free(rtspcld->tcp_socket);
		rtspcld->tcp_socket = NULL;
	}
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
	g_queue_free(rtspcld->received_lines);
	rtspcl_remove_all_exthds(rtspcld);
	g_free(rtspcld->session);
	g_cond_free(rtspcld->cond);
	g_mutex_free(rtspcld->mutex);
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
read_line(struct rtspcl_data *rtspcld, char *line, int maxlen,
	  int timeout)
{
	g_mutex_lock(rtspcld->mutex);

	GTimeVal end_time;
	if (timeout >= 0) {
		g_get_current_time(&end_time);

		end_time.tv_sec += timeout / 1000;
		timeout %= 1000;
		end_time.tv_usec = timeout * 1000;
		if (end_time.tv_usec > 1000000) {
			end_time.tv_usec -= 1000000;
			++end_time.tv_sec;
		}
	}

	while (true) {
		if (!g_queue_is_empty(rtspcld->received_lines)) {
			/* success, copy to buffer */

			char *p = g_queue_pop_head(rtspcld->received_lines);
			g_mutex_unlock(rtspcld->mutex);

			g_strlcpy(line, p, maxlen);
			g_free(p);

			return strlen(line);
		}

		if (rtspcld->tcp_socket == NULL) {
			/* error */
			g_mutex_unlock(rtspcld->mutex);
			return -1;
		}

		if (timeout < 0) {
			g_cond_wait(rtspcld->cond, rtspcld->mutex);
		} else if (!g_cond_timed_wait(rtspcld->cond, rtspcld->mutex,
					      &end_time)) {
			g_mutex_unlock(rtspcld->mutex);
			return 0;
		}
	}
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
	int dsize = 0;
	int timeout = 5000; // msec unit

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

	if (!tcp_socket_send(rtspcld->tcp_socket, req, strlen(req))) {
		g_set_error(error_r, rtsp_client_quark(), errno,
			    "write error: %s",
			    g_strerror(errno));
		return false;
	}

	if (!get_response) return true;

	if (read_line(rtspcld, line, sizeof(line), timeout) <= 0) {
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
	while (read_line(rtspcld, line, sizeof(line), timeout) > 0) {
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
