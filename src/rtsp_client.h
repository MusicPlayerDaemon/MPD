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

#ifndef MPD_RTSP_CLIENT_H
#define MPD_RTSP_CLIENT_H

#include <stdbool.h>
#include <glib.h>

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <netinet/in.h>
#endif

struct key_data {
	char *key;
	char *data;
	struct key_data *next;
};

struct rtspcl_data {
	GMutex *mutex;
	GCond *cond;

	GQueue *received_lines;

	struct tcp_socket *tcp_socket;

	char url[128];
	int cseq;
	struct key_data *exthds;
	char *session;
	char *transport;
	unsigned short server_port;
	unsigned short control_port;
	struct in_addr host_addr;
	struct in_addr local_addr;
	const char *useragent;

};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
rtsp_client_quark(void)
{
	return g_quark_from_static_string("rtsp_client");
}

void
free_kd(struct key_data *kd);

char *
kd_lookup(struct key_data *kd, const char *key);

G_GNUC_MALLOC
struct rtspcl_data *
rtspcl_open(void);

bool
rtspcl_connect(struct rtspcl_data *rtspcld, const char *host, short destport,
	       const char *sid, GError **error_r);

void
rtspcl_close(struct rtspcl_data *rtspcld);

void
rtspcl_add_exthds(struct rtspcl_data *rtspcld, const char *key, char *data);

bool
exec_request(struct rtspcl_data *rtspcld, const char *cmd,
	     const char *content_type, const char *content,
	     int get_response,
	     const struct key_data *hds, struct key_data **kd,
	     GError **error_r);

bool
rtspcl_set_parameter(struct rtspcl_data *rtspcld, const char *parameter,
		     GError **error_r);

void
rtspcl_set_useragent(struct rtspcl_data *rtspcld, const char *name);

bool
rtspcl_announce_sdp(struct rtspcl_data *rtspcld, const char *sdp,
		    GError **error_r);

bool
rtspcl_setup(struct rtspcl_data *rtspcld, struct key_data **kd,
	     int control_port, int ntp_port,
	     GError **error_r);

bool
rtspcl_record(struct rtspcl_data *rtspcld,
	      int seq_num, int rtptime,
	      GError **error_r);

char *
rtspcl_local_ip(struct rtspcl_data *rtspcld);

#endif
