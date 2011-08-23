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

#include "output_api.h"
#include "mixer_list.h"
#include "raop_output_plugin.h"

#include <glib.h>
#include <unistd.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>

#ifndef WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <netdb.h>
#endif

#include <fcntl.h>
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "raop"

static struct raop_session_data *raop_session = NULL;

/**
 * The quark used for GError.domain.
 */
static inline GQuark
raop_output_quark(void)
{
	return g_quark_from_static_string("raop_output");
}

static struct raop_data *
new_raop_data(void)
{
	struct raop_data *ret = g_new(struct raop_data, 1);
	int i;

	ret->control_mutex = g_mutex_new();

	ret->next = NULL;
	ret->is_master = 0;
	ret->started = 0;
	ret->paused = 0;

	if (raop_session == NULL) {
		raop_session = (struct raop_session_data *) malloc(sizeof(struct raop_session_data));
		raop_session->raop_list = NULL;
		raop_session->ntp.port = 6002;
		raop_session->ntp.fd = -1;
		raop_session->ctrl.port = 6001;
		raop_session->ctrl.fd = -1;
		raop_session->play_state.playing = false;
		raop_session->play_state.seq_num = (short) g_random_int();
		raop_session->play_state.rtptime = g_random_int();
		raop_session->play_state.sync_src = g_random_int();
		raop_session->play_state.last_send.tv_sec = 0;
		raop_session->play_state.last_send.tv_usec = 0;

		if (!RAND_bytes(raop_session->encrypt.iv, sizeof(raop_session->encrypt.iv)) || !RAND_bytes(raop_session->encrypt.key, sizeof(raop_session->encrypt.key))) {
			g_warning("%s:RAND_bytes error code=%ld\n",__func__,ERR_get_error());
			return NULL;
		}
		memcpy(raop_session->encrypt.nv, raop_session->encrypt.iv, sizeof(raop_session->encrypt.nv));
		for (i = 0; i < 16; i++) {
			printf("0x%x ", raop_session->encrypt.key[i]);
		}
		printf("\n");
		AES_set_encrypt_key(raop_session->encrypt.key, 128, &raop_session->encrypt.ctx);

		raop_session->data_fd = -1;
		memset(raop_session->buffer, 0, RAOP_BUFFER_SIZE);
		raop_session->bufferSize = 0;

		raop_session->data_mutex = g_mutex_new();
		raop_session->list_mutex = g_mutex_new();
	}

	return ret;
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
 * Free all memory associated with key_data
 */
static void
free_kd(struct key_data *kd)
{
	struct key_data *iter = kd;
	while (iter) {
		free(iter->key);
		if(iter->data) free(iter->data);
		iter = iter->next;
		free(kd);
		kd = iter;
	}
}

/*
 * key_data type data look up
 */
static char *
kd_lookup(struct key_data *kd, const char *key)
{
	while (kd) {
		g_debug("checking key %s %s\n", kd->key, key);
		if (!strcmp(kd->key, key)) {
			g_debug("found %s\n", kd->data);
			return kd->data;
		}
		kd = kd->next;
	}
	return NULL;
}

/*
 * remove one character from a string
 * return the number of deleted characters
 */
static int
remove_char_from_string(char *str, char c)
{
	char *src, *dst;

	/* skip all characters that don't need to be copied */
	src = strchr(str, c);
	if (!src)
		return 0;

	for (dst = src; *src; src++)
		if (*src != c)
			*(dst++) = *src;

	*dst = '\0';

	return src - dst;
}

#define SLEEP_MSEC(val) usleep(val*1000)

/* bind an opened socket to specified hostname and port.
 * if hostname=NULL, use INADDR_ANY.
 * if *port=0, use dynamically assigned port
 */
static int bind_host(int sd, char *hostname, unsigned long ulAddr, unsigned short *port)
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
					g_warning("gethostbyname: '%s' \n", hostname);
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
		g_warning("bind error: %s\n", strerror(errno));
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
static int open_tcp_socket(char *hostname, unsigned short *port)
{
	int sd;

	/* socket creation */
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd < 0) {
		g_warning("cannot create tcp socket\n");
		return -1;
	}
	if (bind_host(sd, hostname,0, port)) {
		close(sd);
		return -1;
	}

	return sd;
}

/*
 * open udp  port
 */
static int open_udp_socket(char *hostname, unsigned short *port)
{
	int sd;
	int size = 30000;

	/* socket creation */
	sd = socket(PF_INET, SOCK_DGRAM, 0);
	if (sd < 0) {
		g_warning("cannot create udp socket\n");
		return -1;
	}
	if (setsockopt(sd, SOL_SOCKET, SO_SNDBUF, (void *) &size, sizeof(size)) < 0) {
		g_warning("Could not set udp send buffer to %d\n", size);
		return -1;
	}
	if (bind_host(sd, hostname,0, port)) {
		close(sd);
		return -1;
	}

	return sd;
}

/*
 * create tcp connection
 * as long as the socket is not non-blocking, this can block the process
 * nsport is network byte order
 */
static bool
get_tcp_connect(int sd, struct sockaddr_in dest_addr)
{
	if (connect(sd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr))){
		SLEEP_MSEC(100L);
		// try one more time
		if (connect(sd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr))) {
			g_warning("error:get_tcp_nconnect addr=%s, port=%d\n",
				  inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
			return false;
		}
	}
	return true;
}

static bool
get_sockaddr_by_host(const char *host, short destport, struct sockaddr_in *addr)
{
	struct hostent *h;

	h = gethostbyname(host);
	if (h) {
		addr->sin_family = h->h_addrtype;
		memcpy((char *) &addr->sin_addr.s_addr, h->h_addr_list[0], h->h_length);
	} else {
		addr->sin_family = AF_INET;
		if ((addr->sin_addr.s_addr=inet_addr(host))==0xFFFFFFFF) {
			g_warning("gethostbyname: '%s' \n", host);
			return false;
		}
	}
	addr->sin_port = htons(destport);
	return true;
}

static bool
get_tcp_connect_by_host(int sd, const char *host, short destport)
{
	struct sockaddr_in addr;

	get_sockaddr_by_host(host, destport, &addr);

	return get_tcp_connect(sd, addr);
}

/*
 * Store time in the NTP format in the buffer
 */
static void
fill_time_buffer_with_time(unsigned char *buffer, struct timeval *tout)
{
	unsigned long secs_to_baseline = 964697997;
	int iter;
	double fraction;
	unsigned long long_fraction;
	unsigned long secs;

	fraction = ((double) tout->tv_usec) / 1000000.0;
	long_fraction = (unsigned long) (fraction * 256.0 * 256.0 * 256.0 * 256.0);
	secs = secs_to_baseline + tout->tv_sec;
	for (iter = 0; iter < 4; iter++) {
		buffer[iter] = (secs >> ((3 - iter) * 8)) & 0xff;
	}
	for (iter = 0; iter < 4; iter++) {
		buffer[4 + iter] = (long_fraction >> ((3 - iter) * 8)) & 0xff;
	}
}

/*
 * Calculate the current NTP time, store it in the buffer.
 */
static void
fill_time_buffer(unsigned char *buffer)
{
	struct timeval current_time;

	gettimeofday(&current_time,NULL);
	fill_time_buffer_with_time(buffer, &current_time);
}

/*
 * Calculate the current NTP time, store it in the buffer.
 */
static void
fill_int(unsigned char *buffer, unsigned int rtp_time)
{
	int iter;
	for (iter = 0; iter < 4; iter++) {
		buffer[iter] = (rtp_time >> ((3 - iter) * 8)) & 0xff;
	}
}

/*
 * Recv the NTP datagram from the AirTunes, send back an NTP response.
 */
static bool
send_timing_response(int fd)
{
	unsigned char buf[32];
	struct sockaddr addr;
	int iter;
	unsigned int addr_len = sizeof(addr);
	int num_bytes = recvfrom(fd, buf, sizeof(buf), 0, &addr, &addr_len);
	if (num_bytes == 0) {
		return false;
	}
	fill_time_buffer(buf + 16);
	// set to response
	buf[1] = 0xd3;
	// copy request
	for (iter = 0; iter < 8; iter++) {
		buf[8 + iter] = buf[24 + iter];
	}
	fill_time_buffer(buf + 24);

	num_bytes = sendto(fd, buf, num_bytes, 0, &addr, addr_len);

	return num_bytes == sizeof(buf);
}

static bool
get_time_for_rtp(struct play_state *state, struct timeval *tout)
{
	unsigned long rtp_diff = state->rtptime - state->start_rtptime;
	unsigned long add_secs = rtp_diff / 44100;
	unsigned long add_usecs = (((rtp_diff % 44100) * 10000) / 441) % 1000000;
	tout->tv_sec = state->start_time.tv_sec + add_secs;
	tout->tv_usec = state->start_time.tv_usec + add_usecs;
	if (tout->tv_usec >= 1000000) {
		tout->tv_sec++;
		tout->tv_usec = tout->tv_usec % 1000000;
	}
	return true;
}

/*
 * Send a control command
 */
static bool
send_control_command(struct control_data *ctrl, struct raop_data *rd,
		     struct play_state *state,
		     GError **error_r)
{
	unsigned char buf[20];
	int diff;
	int num_bytes;
	struct timeval ctrl_time;

	diff = 88200;
	if (rd->started) {
		buf[0] = 0x80;
		diff += NUMSAMPLES;
	} else {
		buf[0] = 0x90;
		state->playing = true;
		state->start_rtptime = state->rtptime;
	}
	buf[1] = 0xd4;
	buf[2] = 0x00;
	buf[3] = 0x07;
	fill_int(buf + 4, state->rtptime - diff);
	get_time_for_rtp(state, &ctrl_time);
	fill_time_buffer_with_time(buf + 8, &ctrl_time);
	fill_int(buf + 16, state->rtptime);

	num_bytes = sendto(ctrl->fd, buf, sizeof(buf), 0, (struct sockaddr *) &rd->ctrl_addr, sizeof(rd->ctrl_addr));
	if (num_bytes < 0) {
		g_set_error(error_r, raop_output_quark(), errno,
			    "Unable to send control command: %s",
			    g_strerror(errno));
		return false;
	}

	return true;
}

/*
 * check to see if there are any timing requests, and respond if there are any
 */
static bool
check_timing(struct timeval *tout)
{
	fd_set rdfds;
	int fdmax = 0;

	FD_ZERO(&rdfds);

	FD_SET(raop_session->ntp.fd, &rdfds);
	fdmax = raop_session->ntp.fd;
	select(fdmax + 1, &rdfds,NULL, NULL, tout);
	if (FD_ISSET(raop_session->ntp.fd, &rdfds)) {
		if (!send_timing_response(raop_session->ntp.fd)) {
			g_debug("unable to send timing response\n");
			return false;
		}
	}
	return true;
}

/*
 * send RTSP request, and get response if it's needed
 * if this gets a success, *kd is allocated or reallocated (if *kd is not NULL)
 */
static bool
exec_request(struct rtspcl_data *rtspcld, const char *cmd,
	     const char *content_type, const char *content,
	     int get_response,
	     const struct key_data *hds, struct key_data **kd)
{
	char line[1024];
	char req[1024];
	char reql[128];
	const char delimiters[] = " ";
	char *token, *dp;
	int i,dsize = 0,rval;
	struct key_data *cur_kd = *kd;
	unsigned int j;
	int timeout = 5000; // msec unit

	fd_set rdfds;
	int fdmax = 0;
	struct timeval tout = {.tv_sec=10, .tv_usec=0};

	if (!rtspcld) return false;

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
	g_debug("sent %s", req);

	if (!get_response) return true;

	while (true) {
		FD_ZERO(&rdfds);
		FD_SET(rtspcld->fd, &rdfds);
		FD_SET(raop_session->ntp.fd, &rdfds);
		fdmax = raop_session->ntp.fd > rtspcld->fd ? raop_session->ntp.fd : rtspcld->fd;;
		select(fdmax + 1, &rdfds, NULL, NULL, &tout);
		if (FD_ISSET(rtspcld->fd, &rdfds)) {
			break;
		}
		if (FD_ISSET(raop_session->ntp.fd, &rdfds)) {
			send_timing_response(raop_session->ntp.fd);
		}
	}

	if (read_line(rtspcld->fd, line, sizeof(line), timeout, 0) <= 0) {
		g_warning("%s: request failed\n",__func__);
		return false;
	}
	g_debug("received %s", line);

	token = strtok(line, delimiters);
	token = strtok(NULL, delimiters);
	if (token == NULL || strcmp(token,"200")) {
		g_warning("%s: request failed, error %s\n", __func__, token);
		return false;
	}

	i = 0;
	while (read_line(rtspcld->fd, line, sizeof(line), timeout, 0) > 0) {
		struct key_data *new_kd = NULL;
		g_debug("%s -\n",line);
		timeout = 1000; // once it started, it shouldn't take a long time
		if (i && line[0] == ' ') {
			for (j = 0; j < strlen(line); j++) if (line[j] != ' ') break;
			dsize += strlen(line + j);
			if ((new_kd->data = realloc(new_kd->data, dsize))) return false;
			strcat(new_kd->data, line + j);
			continue;
		}
		dp = strstr(line, ":");
		if (!dp) {
			g_warning("%s: Request failed, bad header\n", __func__);
			free_kd(*kd);
			*kd = NULL;
			return false;
		}
		*dp = 0;
		new_kd = malloc(sizeof(struct key_data));
		new_kd->key = malloc(strlen(line) + 1);
		strcpy(new_kd->key, line);
		dsize = strlen(dp + 1) + 1;
		new_kd->data = malloc(dsize);
		strcpy(new_kd->data, dp + 1);
		new_kd->next = NULL;
		if (cur_kd == NULL) {
			cur_kd = *kd = new_kd;
		} else {
			cur_kd->next = new_kd;
			cur_kd = new_kd;
		}
		i++;
	}
	return true;
}

static bool
rtspcl_set_parameter(struct rtspcl_data *rtspcld, const char *parameter)
{
	return exec_request(rtspcld, "SET_PARAMETER", "text/parameters",
			    parameter, 1, NULL, &rtspcld->kd);
}

static struct rtspcl_data *
rtspcl_open(void)
{
	struct rtspcl_data *rtspcld;
	rtspcld = malloc(sizeof(struct rtspcl_data));
	memset(rtspcld, 0, sizeof(struct rtspcl_data));
	rtspcld->useragent = "RTSPClient";
	return rtspcld;
}

static bool
rtspcl_remove_all_exthds(struct rtspcl_data *rtspcld)
{
	free_kd(rtspcld->exthds);
	rtspcld->exthds = NULL;
	return true;
}

static bool
rtspcl_disconnect(struct rtspcl_data *rtspcld)
{
	if (rtspcld->fd > 0) close(rtspcld->fd);
	rtspcld->fd = 0;
	return true;
}

static bool
rtspcl_set_useragent(struct rtspcl_data *rtspcld, const char *name)
{
	rtspcld->useragent = name;
	return true;
}

static bool
rtspcl_add_exthds(struct rtspcl_data *rtspcld, const char *key, char *data)
{
	struct key_data *new_kd;
	new_kd = (struct key_data *) malloc(sizeof(struct key_data));
	new_kd->key = malloc(strlen(key) + 1);
	new_kd->data = malloc(strlen(data) + 1);
	strcpy(new_kd->key, key);
	strcpy(new_kd->data, data);
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
	return true;
}

static bool
rtspcl_connect(struct rtspcl_data *rtspcld, const char *host, short destport,
	       const char *sid)
{
	unsigned short myport = 0;
	struct sockaddr_in name;
	socklen_t namelen = sizeof(name);

	if ((rtspcld->fd = open_tcp_socket(NULL, &myport)) == -1) return -1;
	if (!get_tcp_connect_by_host(rtspcld->fd, host, destport)) return -1;
	getsockname(rtspcld->fd, (struct sockaddr*)&name, &namelen);
	memcpy(&rtspcld->local_addr, &name.sin_addr,sizeof(struct in_addr));
	sprintf(rtspcld->url, "rtsp://%s/%s", inet_ntoa(name.sin_addr), sid);
	getpeername(rtspcld->fd, (struct sockaddr*)&name, &namelen);
	memcpy(&rtspcld->host_addr, &name.sin_addr, sizeof(struct in_addr));
	return true;
}

static bool
rtspcl_announce_sdp(struct rtspcl_data *rtspcld, const char *sdp)
{
	return exec_request(rtspcld, "ANNOUNCE", "application/sdp", sdp, 1, NULL, &rtspcld->kd);
}

static bool
rtspcl_setup(struct rtspcl_data *rtspcld, struct key_data **kd)
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
		 raop_session->ctrl.port, raop_session->ntp.port);

	hds.key = transport_key;
	hds.data = transport_value;
	hds.next = NULL;
	if (!exec_request(rtspcld, "SETUP", NULL, NULL, 1, &hds, &rkd)) return false;

	if (!(rtspcld->session = strdup(kd_lookup(rkd, "Session")))) {
		g_warning("%s: no session in response\n",__func__);
		goto erexit;
	}
	if (!(rtspcld->transport = kd_lookup(rkd, "Transport"))) {
		g_warning("%s: no transport in response\n",__func__);
		goto erexit;
	}
	if (!(buf = malloc(strlen(rtspcld->transport) + 1))) {
		goto erexit;
	}
	strcpy(buf, rtspcld->transport);
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
		g_warning("%s: no server_port in response\n",__func__);
		goto erexit;
	}
	if (rtspcld->control_port == 0) {
		g_warning("%s: no control_port in response\n",__func__);
		goto erexit;
	}
	rval = true;
 erexit:
	if (buf) free(buf);
	if (!rval) {
		free_kd(rkd);
		rkd = NULL;
	}
	*kd = rkd;
	return rval;
}

static bool
rtspcl_record(struct rtspcl_data *rtspcld)
{
	if (!rtspcld->session) {
		g_warning("%s: no session in progress\n", __func__);
		return false;
	}

	char buf[128];
	sprintf(buf, "seq=%d,rtptime=%u", raop_session->play_state.seq_num, raop_session->play_state.rtptime);

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
			    &rtspcld->kd);
}

static bool
rtspcl_close(struct rtspcl_data *rtspcld)
{
	rtspcl_disconnect(rtspcld);
	rtspcl_remove_all_exthds(rtspcld);
	free(rtspcld->session);
	free(rtspcld);
	return true;
}

static char* rtspcl_local_ip(struct rtspcl_data *rtspcld)
{
	return inet_ntoa(rtspcld->local_addr);
}

static int rsa_encrypt(const unsigned char *text, int len, unsigned char *res)
{
	RSA *rsa;
	gsize usize;
	unsigned char *modulus;
	unsigned char *exponent;
	int size;

	char n[] =
		"59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUtwC"
		"5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDR"
		"KSKv6kDqnw4UwPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuB"
		"OitnZ/bDzPHrTOZz0Dew0uowxf/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJ"
		"Q+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/UAaHqn9JdsBWLUEpVviYnh"
		"imNVvYFZeCXg/IdTQ+x4IRdiXNv5hEew==";
	char e[] = "AQAB";

	rsa = RSA_new();

	modulus = g_base64_decode(n, &usize);
	rsa->n = BN_bin2bn(modulus, usize, NULL);
	exponent = g_base64_decode(e, &usize);
	rsa->e = BN_bin2bn(exponent, usize, NULL);
	g_free(modulus);
	g_free(exponent);
	size = RSA_public_encrypt(len, text, res, rsa, RSA_PKCS1_OAEP_PADDING);

	RSA_free(rsa);
	return size;
}

static int
raop_encrypt(struct encrypt_data *encryp, unsigned char *data, int size)
{
	// any bytes that fall beyond the last 16 byte page should be sent
	// in the clear
	int alt_size = size - (size % 16);

	memcpy(encryp->nv, encryp->iv, 16);

	AES_cbc_encrypt(data, data, alt_size, &encryp->ctx, encryp->nv, 1);

	return size;
}

/* write bits filed data, *bpos=0 for msb, *bpos=7 for lsb
   d=data, blen=length of bits field
*/
static inline void
bits_write(unsigned char **p, unsigned char d, int blen, int *bpos)
{
	int lb, rb, bd;
	lb =7 - *bpos;
	rb = lb - blen + 1;
	if (rb >= 0) {
		bd = d << rb;
		if (*bpos)
			**p |= bd;
		else
			**p = bd;
		*bpos += blen;
	} else {
		bd = d >> -rb;
		**p |= bd;
		*p += 1;
		**p = d << (8 + rb);
		*bpos = -rb;
	}
}

static bool
wrap_pcm(unsigned char *buffer, int bsize, int *size, unsigned char *inData, int inSize)
{
	unsigned char one[4];
	int count = 0;
	int bpos = 0;
	unsigned char *bp = buffer;
	int i, nodata = 0;
	bits_write(&bp, 1, 3, &bpos); // channel=1, stereo
	bits_write(&bp, 0, 4, &bpos); // unknown
	bits_write(&bp, 0, 8, &bpos); // unknown
	bits_write(&bp, 0, 4, &bpos); // unknown
	if (bsize != 4096 && false)
		bits_write(&bp, 1, 1, &bpos); // hassize
	else
		bits_write(&bp, 0, 1, &bpos); // hassize
	bits_write(&bp, 0, 2, &bpos); // unused
	bits_write(&bp, 1, 1, &bpos); // is-not-compressed
	if (bsize != 4096 && false) {
		// size of data, integer, big endian
		bits_write(&bp, (bsize >> 24) & 0xff, 8, &bpos);
		bits_write(&bp, (bsize >> 16) & 0xff, 8, &bpos);
		bits_write(&bp, (bsize >> 8) & 0xff, 8, &bpos);
		bits_write(&bp, bsize&0xff, 8, &bpos);
	}
	while (1) {
		if (inSize <= count * 4) nodata = 1;
		if (nodata) break;
		one[0] = inData[count * 4];
		one[1] = inData[count * 4 + 1];
		one[2] = inData[count * 4 + 2];
		one[3] = inData[count * 4 + 3];

#if BYTE_ORDER == BIG_ENDIAN
		bits_write(&bp, one[0], 8, &bpos);
		bits_write(&bp, one[1], 8, &bpos);
		bits_write(&bp, one[2], 8, &bpos);
		bits_write(&bp, one[3], 8, &bpos);
#else
		bits_write(&bp, one[1], 8, &bpos);
		bits_write(&bp, one[0], 8, &bpos);
		bits_write(&bp, one[3], 8, &bpos);
		bits_write(&bp, one[2], 8, &bpos);
#endif

		if (++count == bsize) break;
	}
	if (!count) return false; // when no data at all, it should stop playing
	/* when readable size is less than bsize, fill 0 at the bottom */
	for(i = 0; i < (bsize - count) * 4; i++) {
		bits_write(&bp, 0, 8, &bpos);
	}
	*size = (int)(bp - buffer);
	if (bpos) *size += 1;
	return true;
}

static bool
raopcl_stream_connect(G_GNUC_UNUSED struct raop_data *rd)
{
	return true;
}


static bool
raopcl_connect(struct raop_data *rd)
{
	unsigned char buf[4 + 8 + 16];
	char sid[16];
	char sci[24];
	char act_r[17];
	char *sac=NULL, *key = NULL, *iv = NULL;
	char sdp[1024];
	int rval = false;
	struct key_data *setup_kd = NULL;
	char *aj, *token, *pc;
	const char delimiters[] = ";";
	unsigned char rsakey[512];
	struct timeval current_time;
	unsigned int sessionNum;
	int i;


	gettimeofday(&current_time,NULL);
	sessionNum = current_time.tv_sec + 2082844804;

	RAND_bytes(buf, sizeof(buf));
	sprintf(act_r, "%u", (unsigned int) g_random_int());
	sprintf(sid, "%u", sessionNum);
	sprintf(sci, "%08x%08x", *((int *)(buf + 4)), *((int *)(buf + 8)));
	sac = g_base64_encode(buf + 12, 16);
	if (!(rd->rtspcl = rtspcl_open())) goto erexit;
	if (!rtspcl_set_useragent(rd->rtspcl, "iTunes/8.1.1 (Macintosh; U; PPC Mac OS X 10.4)")) goto erexit;
	if (!rtspcl_add_exthds(rd->rtspcl, "Client-Instance", sci)) goto erexit;
	if (!rtspcl_add_exthds(rd->rtspcl, "DACP-ID", sci)) goto erexit;
	if (!rtspcl_add_exthds(rd->rtspcl, "Active-Remote", act_r)) goto erexit;
	if (!rtspcl_connect(rd->rtspcl, rd->addr, rd->rtsp_port, sid)) goto erexit;

	i = rsa_encrypt(raop_session->encrypt.key, 16, rsakey);
	key = g_base64_encode(rsakey, i);
	remove_char_from_string(key, '=');
	iv = g_base64_encode(raop_session->encrypt.iv, 16);
	remove_char_from_string(iv, '=');
	sprintf(sdp,
		"v=0\r\n"
		"o=iTunes %s 0 IN IP4 %s\r\n"
		"s=iTunes\r\n"
		"c=IN IP4 %s\r\n"
		"t=0 0\r\n"
		"m=audio 0 RTP/AVP 96\r\n"
		"a=rtpmap:96 AppleLossless\r\n"
		"a=fmtp:96 %d  0 16 40 10 14 2 255 0 0 44100\r\n"
		"a=rsaaeskey:%s\r\n"
		"a=aesiv:%s\r\n",
		sid, rtspcl_local_ip(rd->rtspcl), rd->addr, NUMSAMPLES, key, iv);
	remove_char_from_string(sac, '=');
	//	if (!rtspcl_add_exthds(rd->rtspcl, "Apple-Challenge", sac)) goto erexit;
	if (!rtspcl_announce_sdp(rd->rtspcl, sdp)) goto erexit;
	//	if (!rtspcl_mark_del_exthds(rd->rtspcl, "Apple-Challenge")) goto erexit;
	if (!rtspcl_setup(rd->rtspcl, &setup_kd)) goto erexit;
	if (!(aj = kd_lookup(setup_kd,"Audio-Jack-Status"))) {
		g_warning("%s: Audio-Jack-Status is missing\n",__func__);
		goto erexit;
	}

	token = strtok(aj, delimiters);
	while (token) {
		if ((pc = strstr(token,"="))) {
			*pc = 0;
			if (!strcmp(token,"type") && !strcmp(pc+1,"digital")) {
				//				rd->ajtype = JACK_TYPE_DIGITAL;
			}
		} else {
			if (!strcmp(token,"connected")) {
				//				rd->ajstatus = JACK_STATUS_CONNECTED;
			}
		}
		token = strtok(NULL, delimiters);
	}

	if (!get_sockaddr_by_host(rd->addr, rd->rtspcl->control_port, &rd->ctrl_addr)) goto erexit;
	if (!get_sockaddr_by_host(rd->addr, rd->rtspcl->server_port, &rd->data_addr)) goto erexit;

	if (!rtspcl_record(rd->rtspcl)) goto erexit;

	if (!raopcl_stream_connect(rd)) goto erexit;

	rval = true;

 erexit:
	if (sac) g_free(sac);
	if (key) g_free(key);
	if (iv) g_free(iv);
	free_kd(setup_kd);
	return rval;
}

static bool
raopcl_close(struct raop_data *rd)
{
	if (rd->rtspcl)
		rtspcl_close(rd->rtspcl);
	rd->rtspcl = NULL;
	free(rd);
	return true;
}

static int
difference (struct timeval *t1, struct timeval *t2)
{
	int ret = 150000000;
	if (t1->tv_sec - t2->tv_sec < 150) {
		ret = (t1->tv_sec - t2->tv_sec) * 1000000;
		ret += t1->tv_usec - t2->tv_usec;
	}
	return ret;
}

/*
 * With airtunes version 2, we don't get responses back when we send audio
 * data.  The only requests we get from the airtunes device are timing
 * requests.
 */
static bool
send_audio_data(int fd)
{
	int i = 0;
	struct timeval current_time, tout, rtp_time;
	struct raop_data *rd = raop_session->raop_list;

	get_time_for_rtp(&raop_session->play_state, &rtp_time);
	gettimeofday(&current_time, NULL);
	int diff = difference(&current_time, &rtp_time);

	while (diff < -10000) {
		tout.tv_sec = 0;
		tout.tv_usec = -diff;
		check_timing(&tout);
		gettimeofday(&current_time, NULL);
		diff = difference(&current_time, &rtp_time);
	}
	gettimeofday(&raop_session->play_state.last_send, NULL);
	while (rd) {
		if (rd->started) {
			raop_session->data[1] = 0x60;
		} else {
			rd->started = true;
			raop_session->data[1] = 0xe0;
		}
		i = sendto(fd, raop_session->data + raop_session->wblk_wsize,
			   raop_session->wblk_remsize, 0, (struct sockaddr *) &rd->data_addr,
			   sizeof(rd->data_addr));
		if (i < 0) {
			g_warning("%s: write error: %s\n", __func__, strerror(errno));
			return false;
		}
		if (i == 0) {
			g_warning("%s: write, disconnected on the other end\n", __func__);
			return false;
		}
		rd = rd->next;
	}
	raop_session->wblk_wsize += i;
	raop_session->wblk_remsize -= i;

	//g_debug("%d bytes are sent, remaining size=%d\n",i,rd->wblk_remsize);
	return true;
}

static void *
raop_output_init(G_GNUC_UNUSED const struct audio_format *audio_format,
		 G_GNUC_UNUSED const struct config_param *param,
		 G_GNUC_UNUSED GError **error)
{
	struct raop_data *rd;

	rd = new_raop_data();
	rd->addr = config_get_block_string(param, "host", NULL);
	rd->rtsp_port = config_get_block_unsigned(param, "port", 5000);
	rd->volume = config_get_block_unsigned(param, "volume", 75);
	return rd;
}

static bool
raop_set_volume_local(struct raop_data *rd, int volume)
{
	char vol_str[128];
	sprintf(vol_str, "volume: %d.000000\r\n", volume);
	return rtspcl_set_parameter(rd->rtspcl, vol_str);
}


static void
raop_output_finish(void *data)
{
	struct raop_data *rd = data;
	raopcl_close(rd);
	g_mutex_free(rd->control_mutex);
}

#define RAOP_VOLUME_MIN -30
#define RAOP_VOLUME_MAX 0

int
raop_get_volume(struct raop_data *rd)
{
	return rd->volume;
}

bool
raop_set_volume(struct raop_data *rd, unsigned volume)
{
	int raop_volume;
	bool rval;

	//set parameter volume
	if (volume == 0) {
		raop_volume = -144;
	} else {
		raop_volume = RAOP_VOLUME_MIN +
			(RAOP_VOLUME_MAX - RAOP_VOLUME_MIN) * volume / 100;
	}
	g_mutex_lock(rd->control_mutex);
	rval = raop_set_volume_local(rd, raop_volume);
	if (rval) rd->volume = volume;
	g_mutex_unlock(rd->control_mutex);

	return rval;
}

static void
raop_output_cancel(void *data)
{
	//flush
	struct key_data kd;
	struct raop_data *rd = (struct raop_data *) data;
	int flush_diff = 1;

	rd->started = 0;
	if (rd->is_master) {
		raop_session->play_state.playing = false;
	}
	if (rd->paused) {
		return;
	}

	g_mutex_lock(rd->control_mutex);
	static char rtp_key[] = "RTP-Info";
	kd.key = rtp_key;
	char buf[128];
	sprintf(buf, "seq=%d; rtptime=%d", raop_session->play_state.seq_num + flush_diff, raop_session->play_state.rtptime + NUMSAMPLES * flush_diff);
	kd.data = buf;
	kd.next = NULL;
	exec_request(rd->rtspcl, "FLUSH", NULL, NULL, 1, &kd, &(rd->rtspcl->kd));
	g_mutex_unlock(rd->control_mutex);
}

static bool
raop_output_pause(void *data)
{
	struct timeval tout = {.tv_sec = 0, .tv_usec = 0};
	struct raop_data *rd = (struct raop_data *) data;

	check_timing(&tout);
	rd->paused = true;
	return true;
}

static void
raop_output_close(void *data)
{
	//teardown
	struct raop_data *rd = data;
	struct raop_data *iter = raop_session->raop_list;
	struct raop_data *prev = NULL;

	g_mutex_lock(raop_session->list_mutex);
	while (iter) {
		if (iter == rd) {
			if (prev != NULL) {
				prev->next = rd->next;
			} else {
				raop_session->raop_list = rd->next;
				if (raop_session->raop_list == NULL) {
					// TODO clean up everything else
					raop_session->play_state.playing = false;
					close(raop_session->data_fd);
					close(raop_session->ntp.fd);
					close(raop_session->ctrl.fd);
				}
			}
			if (rd->is_master && raop_session->raop_list) {
				raop_session->raop_list->is_master = true;
			}
			rd->next = NULL;
			rd->is_master = false;
			break;
		}
		prev = iter;
		iter = iter->next;
	}
	g_mutex_unlock(raop_session->list_mutex);

	g_mutex_lock(rd->control_mutex);
	exec_request(rd->rtspcl, "TEARDOWN", NULL, NULL, 0, NULL, &(rd->rtspcl->kd));
	g_mutex_unlock(rd->control_mutex);

	rd->started = 0;
}


static bool
raop_output_open(void *data, struct audio_format *audio_format, GError **error_r)
{
	//setup, etc.
	struct raop_data *rd = data;

	g_mutex_lock(raop_session->list_mutex);
	if (raop_session->raop_list == NULL) {
		// first raop, need to initialize session data
		unsigned short myport = 0;
		raop_session->raop_list = rd;
		rd->is_master = true;

		if ((raop_session->data_fd = open_udp_socket(NULL, &myport)) == -1) return -1;
		if ((raop_session->ntp.fd = open_udp_socket(NULL, &raop_session->ntp.port)) == -1) return false;
		if ((raop_session->ctrl.fd = open_udp_socket(NULL, &raop_session->ctrl.port)) == -1) {
			close(raop_session->ntp.fd);
			raop_session->ctrl.fd = -1;
			g_mutex_unlock(raop_session->list_mutex);
			return false;
		}
	}
	g_mutex_unlock(raop_session->list_mutex);

	audio_format->format = SAMPLE_FORMAT_S16;
	g_debug("raop_openDevice %s %d\n", rd->addr, rd->rtsp_port);
	if (!raopcl_connect(rd)) {
		g_set_error(error_r, raop_output_quark(), -1,
			    "Unable to connect to device");
		return false;
	}
	if (!raop_set_volume(rd, rd->volume)) {
		g_set_error(error_r, raop_output_quark(), -1,
			    "Unable to set volume after connecting to device");
		return false;
	}

	g_mutex_lock(raop_session->list_mutex);
	if (!rd->is_master) {
		rd->next = raop_session->raop_list;
		raop_session->raop_list = rd;
	}
	g_mutex_unlock(raop_session->list_mutex);
	return true;
}

static size_t
raop_output_play(void *data, const void *chunk, size_t size,
		 GError **error_r)
{
	//raopcl_send_sample
	struct raop_data *rd = data;
	struct timeval tout = {.tv_sec = 0, .tv_usec = 0};
	size_t rval = 0, orig_size = size;

	rd->paused = false;
	if (!rd->is_master) {
		// only process data for the master raop
		return size;
	}

	g_mutex_lock(raop_session->data_mutex);

	check_timing(&tout);

	if (raop_session->play_state.rtptime <= NUMSAMPLES) {
		// looped over, need new reference point to calculate correct times
		raop_session->play_state.playing = false;
	}

	while (raop_session->bufferSize + size >= RAOP_BUFFER_SIZE) {
		// ntp header
		unsigned char header[] = {
			0x80, 0x60, 0x00, 0x00,
			// rtptime
			0x00, 0x00, 0x00, 0x00,
			// device
			0x7e, 0xad, 0xd2, 0xd3,
		};


		int count = 0;
		int copyBytes = RAOP_BUFFER_SIZE - raop_session->bufferSize;

		if (!raop_session->play_state.playing ||
		    raop_session->play_state.seq_num % (44100 / NUMSAMPLES + 1) == 0) {
			struct raop_data *iter;
			g_mutex_lock(raop_session->list_mutex);
			if (!raop_session->play_state.playing) {
				gettimeofday(&raop_session->play_state.start_time,NULL);
			}
			iter = raop_session->raop_list;
			while (iter) {
				if (!send_control_command(&raop_session->ctrl, iter,
							  &raop_session->play_state,
							  error_r))
					goto erexit;

				iter = iter->next;
			}
			g_mutex_unlock(raop_session->list_mutex);
		}

		fill_int(header + 8, raop_session->play_state.sync_src);

		memcpy(raop_session->buffer + raop_session->bufferSize, chunk, copyBytes);
		raop_session->bufferSize += copyBytes;
		chunk = ((const char *)chunk) + copyBytes;
		size -= copyBytes;

		if (!wrap_pcm(raop_session->data + RAOP_HEADER_SIZE, NUMSAMPLES, &count, raop_session->buffer, RAOP_BUFFER_SIZE)) {
			g_warning("unable to encode %d bytes properly\n", RAOP_BUFFER_SIZE);
		}

		memcpy(raop_session->data, header, RAOP_HEADER_SIZE);
		raop_session->data[2] = raop_session->play_state.seq_num >> 8;
		raop_session->data[3] = raop_session->play_state.seq_num & 0xff;
		raop_session->play_state.seq_num ++;

		fill_int(raop_session->data + 4, raop_session->play_state.rtptime);
		raop_session->play_state.rtptime += NUMSAMPLES;

		raop_encrypt(&raop_session->encrypt, raop_session->data + RAOP_HEADER_SIZE, count);
		raop_session->wblk_remsize = count + RAOP_HEADER_SIZE;
		raop_session->wblk_wsize = 0;

		if (!send_audio_data(raop_session->data_fd)) {
			g_set_error(error_r, raop_output_quark(), -1,
				    "Unable to write to device");
			goto erexit;
		}

		raop_session->bufferSize = 0;
	}
	if (size > 0) {
		memcpy(raop_session->buffer + raop_session->bufferSize, chunk, size);
		raop_session->bufferSize += size;
	}
	rval = orig_size;
 erexit:
	g_mutex_unlock(raop_session->data_mutex);
	return rval;
}

const struct audio_output_plugin raopPlugin = {
	.name = "raop",
	.init = raop_output_init,
	.finish = raop_output_finish,
	.open = raop_output_open,
	.play = raop_output_play,
	.cancel = raop_output_cancel,
	.pause = raop_output_pause,
	.close = raop_output_close,
	.mixer_plugin = &raop_mixer_plugin,
};
