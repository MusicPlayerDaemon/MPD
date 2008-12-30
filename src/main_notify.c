/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 Warren Dukes <warren.dukes@gmail.com>
 * Copyright (C) 2008 Eric Wong <normalperson@yhbt.net>
 *
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

#include "main_notify.h"
#include "utils.h"
#include "ioops.h"
#include "log.h"

#include <assert.h>
#include <glib.h>
#include <string.h>

static struct ioOps main_notify_IO;
static int main_pipe[2];
GThread *main_task;

static int ioops_fdset(fd_set * rfds,
                       G_GNUC_UNUSED fd_set * wfds,
		       G_GNUC_UNUSED fd_set * efds)
{
	FD_SET(main_pipe[0], rfds);
	return main_pipe[0];
}

static void consume_pipe(void)
{
	char buffer[256];
	ssize_t r = read(main_pipe[0], buffer, sizeof(buffer));

	if (r < 0 && errno != EAGAIN && errno != EINTR)
		FATAL("error reading from pipe: %s\n", strerror(errno));
}

static int ioops_consume(int fd_count, fd_set * rfds,
                         G_GNUC_UNUSED fd_set * wfds,
			 G_GNUC_UNUSED fd_set * efds)
{
	if (FD_ISSET(main_pipe[0], rfds)) {
		consume_pipe();
		FD_CLR(main_pipe[0], rfds);
		fd_count--;
	}
	return fd_count;
}

void init_main_notify(void)
{
	main_task = g_thread_self();
	init_async_pipe(main_pipe);
	main_notify_IO.fdset = ioops_fdset;
	main_notify_IO.consume = ioops_consume;
	registerIO(&main_notify_IO);
	main_task = g_thread_self();
}

void deinit_main_notify(void)
{
	deregisterIO(&main_notify_IO);
	xclose(main_pipe[0]);
	xclose(main_pipe[1]);
}

void wakeup_main_task(void)
{
	ssize_t w = write(main_pipe[1], "", 1);
	if (w < 0 && errno != EAGAIN && errno != EINTR)
		g_error("error writing to pipe: %s", strerror(errno));
}

void main_notify_lock(void)
{
	assert(main_task == g_thread_self());
}

void main_notify_unlock(void)
{
	assert(main_task == g_thread_self());
}

void wait_main_task(void)
{
	fd_set rfds;
	int ret;

	assert(main_task == g_thread_self());

	do {
		FD_ZERO(&rfds);
		FD_SET(main_pipe[0], &rfds);
		ret = select(main_pipe[0] + 1, &rfds, NULL, NULL, NULL);
	} while (ret == 0 || (ret < 0 && (errno == EAGAIN || errno == EINTR)));

	if (ret < 0)
		g_error("select() failed: %s", strerror(errno));

	consume_pipe();
}
