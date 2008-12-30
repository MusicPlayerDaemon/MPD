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
#include "log.h"

#include <assert.h>
#include <glib.h>
#include <string.h>

static int main_pipe[2];
GThread *main_task;

static void consume_pipe(void)
{
	char buffer[256];
	ssize_t r = read(main_pipe[0], buffer, sizeof(buffer));

	if (r < 0 && errno != EAGAIN && errno != EINTR)
		FATAL("error reading from pipe: %s\n", strerror(errno));
}

static gboolean
main_notify_event(G_GNUC_UNUSED GIOChannel *source,
		  G_GNUC_UNUSED GIOCondition condition,
		  G_GNUC_UNUSED gpointer data)
{
	consume_pipe();
	main_notify_triggered();
	return true;
}

void init_main_notify(void)
{
	GIOChannel *channel;

	main_task = g_thread_self();

	if (pipe(main_pipe) < 0)
		g_error("Couldn't open pipe: %s", strerror(errno));
	if (set_nonblocking(main_pipe[1]) < 0)
		g_error("Couldn't set non-blocking I/O: %s", strerror(errno));

	channel = g_io_channel_unix_new(main_pipe[0]);
	g_io_add_watch(channel, G_IO_IN, main_notify_event, NULL);
	g_io_channel_unref(channel);

	main_task = g_thread_self();
}

void deinit_main_notify(void)
{
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
	consume_pipe();
}
