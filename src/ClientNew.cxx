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
#include "ClientList.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "system/fd_util.h"
#include "system/Resolver.hxx"
#include "Permission.hxx"
#include "util/Error.hxx"

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

Client::Client(EventLoop &_loop, Partition &_partition,
	       int _fd, int _uid, int _num)
	:FullyBufferedSocket(_fd, _loop, 16384, client_max_output_buffer_size),
	 TimeoutMonitor(_loop),
	 partition(_partition),
	 playlist(partition.playlist), player_control(&partition.pc),
	 permission(getDefaultPermissions()),
	 uid(_uid),
	 num(_num),
	 idle_waiting(false), idle_flags(0),
	 num_subscriptions(0)
{
	TimeoutMonitor::ScheduleSeconds(client_timeout);
}

void
client_new(EventLoop &loop, Partition &partition,
	   int fd, const struct sockaddr *sa, size_t sa_length, int uid)
{
	static unsigned int next_client_num;
	char *remote;

	assert(fd >= 0);

#ifdef HAVE_LIBWRAP
	if (sa->sa_family != AF_UNIX) {
		char *hostaddr = sockaddr_to_string(sa, sa_length,
						    IgnoreError());
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

	ClientList &client_list = *partition.instance.client_list;
	if (client_list.IsFull()) {
		g_warning("Max Connections Reached!");
		close_socket(fd);
		return;
	}

	Client *client = new Client(loop, partition, fd, uid,
				    next_client_num++);

	(void)send(fd, GREETING, sizeof(GREETING) - 1, 0);

	client_list.Add(*client);

	remote = sockaddr_to_string(sa, sa_length, IgnoreError());
	g_log(G_LOG_DOMAIN, LOG_LEVEL_SECURE,
	      "[%u] opened from %s", client->num, remote);
	g_free(remote);
}

void
Client::Close()
{
	partition.instance.client_list->Remove(*this);

	SetExpired();

	g_log(G_LOG_DOMAIN, LOG_LEVEL_SECURE, "[%u] closed", num);
	delete this;
}
