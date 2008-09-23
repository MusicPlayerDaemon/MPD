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
#include "gcc.h"
#include "log.h"

static struct ioOps main_notify_IO;
static int main_pipe[2];
static pthread_t main_task;
static Notify main_notify;
static pthread_mutex_t select_mutex = PTHREAD_MUTEX_INITIALIZER;

static int ioops_fdset(fd_set * rfds,
                       mpd_unused fd_set * wfds, mpd_unused fd_set * efds)
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
                         mpd_unused fd_set * wfds, mpd_unused fd_set * efds)
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
	init_async_pipe(main_pipe);
	main_notify_IO.fdset = ioops_fdset;
	main_notify_IO.consume = ioops_consume;
	registerIO(&main_notify_IO);
	main_task = pthread_self();
	notify_init(&main_notify);
}

static int wakeup_via_pipe(void)
{
	int ret = pthread_mutex_trylock(&select_mutex);
	if (ret == EBUSY) {
		ssize_t w = write(main_pipe[1], "", 1);
		if (w < 0 && errno != EAGAIN && errno != EINTR)
			FATAL("error writing to pipe: %s\n",
			      strerror(errno));
		return 1;
	} else {
		pthread_mutex_unlock(&select_mutex);
		return 0;
	}
}

void wakeup_main_task(void)
{
	assert(!pthread_equal(main_task, pthread_self()));

	main_notify.pending = 1;

	if (!wakeup_via_pipe())
		notify_signal(&main_notify);
}

void main_notify_lock(void)
{
	assert(pthread_equal(main_task, pthread_self()));
	pthread_mutex_lock(&select_mutex);
}

void main_notify_unlock(void)
{
	assert(pthread_equal(main_task, pthread_self()));
	pthread_mutex_unlock(&select_mutex);
}

void wait_main_task(void)
{
	assert(pthread_equal(main_task, pthread_self()));

	notify_enter(&main_notify);
	notify_wait(&main_notify);
	notify_leave(&main_notify);
}

