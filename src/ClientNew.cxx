/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "ClientInternal.hxx"
#include "fd_util.h"
extern "C" {
#include "fifo_buffer.h"
#include "resolver.h"
}
#include "Permission.hxx"
#include "glib_socket.h"

#include <assert.h>
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <unistd.h>

#ifdef HAVE_LIBWRAP
#include <tcpd.h>
#endif


#define LOG_LEVEL_SECURE G_LOG_LEVEL_INFO

static const char GREETING[] = "OK MPD " PROTOCOL_VERSION "\n";

Client::Client(struct player_control *_player_control,
	       int fd, int _uid, int _num)
	:player_control(_player_control),
	 input(fifo_buffer_new(4096)),
	 permission(getDefaultPermissions()),
	 uid(_uid),
	 last_activity(g_timer_new()),
	 cmd_list(nullptr), cmd_list_OK(-1), cmd_list_size(0),
	 deferred_send(g_queue_new()), deferred_bytes(0),
	 num(_num),
	 send_buf_used(0),
	 idle_waiting(false), idle_flags(0)
{
	assert(fd >= 0);

	channel = g_io_channel_new_socket(fd);
	/* GLib is responsible for closing the file descriptor */
	g_io_channel_set_close_on_unref(channel, true);
	/* NULL encoding means the stream is binary safe; the MPD
	   protocol is UTF-8 only, but we are doing this call anyway
	   to prevent GLib from messing around with the stream */
	g_io_channel_set_encoding(channel, NULL, NULL);
	/* we prefer to do buffering */
	g_io_channel_set_buffered(channel, false);

	source_id = g_io_add_watch(channel,
				   GIOCondition(G_IO_IN|G_IO_ERR|G_IO_HUP),
				   client_in_event, this);
}

static void
deferred_buffer_free(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct deferred_buffer *buffer = (struct deferred_buffer *)data;
	g_free(buffer);
}

Client::~Client()
{
	g_timer_destroy(last_activity);

	if (cmd_list != nullptr)
		free_cmd_list(cmd_list);

	g_queue_foreach(deferred_send, deferred_buffer_free, NULL);
	g_queue_free(deferred_send);

	fifo_buffer_free(input);
}

void
client_new(struct player_control *player_control,
	   int fd, const struct sockaddr *sa, size_t sa_length, int uid)
{
	static unsigned int next_client_num;
	char *remote;

	assert(player_control != NULL);
	assert(fd >= 0);

#ifdef HAVE_LIBWRAP
	if (sa->sa_family != AF_UNIX) {
		char *hostaddr = sockaddr_to_string(sa, sa_length, NULL);
		const char *progname = g_get_prgname();

		struct request_info req;
		request_init(&req, RQ_FILE, fd, RQ_DAEMON, progname, 0);

		fromhost(&req);

		if (!hosts_access(&req)) {
			/* tcp wrappers says no */
			g_log(G_LOG_DOMAIN, LOG_LEVEL_SECURE,
			      "libwrap refused connection (libwrap=%s) from %s",
			      progname, hostaddr);

			g_free(hostaddr);
			close_socket(fd);
			return;
		}

		g_free(hostaddr);
	}
#endif	/* HAVE_WRAP */

	if (client_list_is_full()) {
		g_warning("Max Connections Reached!");
		close_socket(fd);
		return;
	}

	Client *client = new Client(player_control, fd, uid,
				    next_client_num++);

	(void)send(fd, GREETING, sizeof(GREETING) - 1, 0);

	client_list_add(client);

	remote = sockaddr_to_string(sa, sa_length, NULL);
	g_log(G_LOG_DOMAIN, LOG_LEVEL_SECURE,
	      "[%u] opened from %s", client->num, remote);
	g_free(remote);
}

void
client_close(Client *client)
{
	client_list_remove(client);

	client_set_expired(client);

	g_log(G_LOG_DOMAIN, LOG_LEVEL_SECURE,
	      "[%u] closed", client->num);
	delete client;
}
