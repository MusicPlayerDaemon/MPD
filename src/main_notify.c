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
#include "notify.h"
#include "utils.h"
#include "ioops.h"
#include "log.h"

#include <assert.h>
#include <glib.h>
#include <string.h>

static struct ioOps main_notify_IO;
static int main_pipe[2];
GThread *main_task;
static struct notify main_notify;
static GMutex *select_mutex = NULL;

static int ioops_fdset(fd_set * rfds,
                       G_GNUC_UNUSED fd_set * wfds,
		       G_GNUC_UNUSED fd_set * efds)
{
	FD_SET(main_pipe[0], rfds);
	return main_pipe[0];
}

static void consume_pipe(void)
{
	char buffer[2];
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
	g_assert(select_mutex == NULL);
	select_mutex = g_mutex_new();
	main_task = g_thread_self();
	init_async_pipe(main_pipe);
	main_notify_IO.fdset = ioops_fdset;
	main_notify_IO.consume = ioops_consume;
	registerIO(&main_notify_IO);
	main_task = g_thread_self();
	notify_init(&main_notify);
}

void deinit_main_notify(void)
{
	notify_deinit(&main_notify);
	deregisterIO(&main_notify_IO);
	xclose(main_pipe[0]);
	xclose(main_pipe[1]);
	g_assert(select_mutex != NULL);
	g_mutex_free(select_mutex);
	select_mutex = NULL;
}

static int wakeup_via_pipe(void)
{
	gboolean ret = g_mutex_trylock(select_mutex);
	if (ret == FALSE) {
		ssize_t w = write(main_pipe[1], "", 1);
		if (w < 0 && errno != EAGAIN && errno != EINTR)
			FATAL("error writing to pipe: %s\n",
			      strerror(errno));
		return 1;
	} else {
		g_mutex_unlock(select_mutex);
		return 0;
	}
}

void wakeup_main_task(void)
{
	main_notify.pending = 1;

	if (!wakeup_via_pipe())
		notify_signal(&main_notify);
}

void main_notify_lock(void)
{
	assert(main_task == g_thread_self());
	g_mutex_lock(select_mutex);
}

void main_notify_unlock(void)
{
	assert(main_task == g_thread_self());
	g_mutex_unlock(select_mutex);
}

void wait_main_task(void)
{
	assert(main_task == g_thread_self());

	notify_wait(&main_notify);
}

