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
#include "GlobalEvents.hxx"
#include "thread/Mutex.hxx"
#include "fd_util.h"
#include "mpd_error.h"

#include <assert.h>
#include <glib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "global_events"

namespace GlobalEvents {
	static int fds[2];
	static GIOChannel *channel;
	static guint source_id;
	static Mutex mutex;
	static bool flags[MAX];
	static Handler handlers[MAX];
}

/**
 * Invoke the callback for a certain event.
 */
static void
InvokeGlobalEvent(GlobalEvents::Event event)
{
	assert((unsigned)event < GlobalEvents::MAX);
	assert(GlobalEvents::handlers[event] != NULL);

	GlobalEvents::handlers[event]();
}

static gboolean
GlobalEventCallback(G_GNUC_UNUSED GIOChannel *source,
		    G_GNUC_UNUSED GIOCondition condition,
		    G_GNUC_UNUSED gpointer data)
{
	char buffer[256];
	gsize bytes_read;
	GError *error = NULL;
	GIOStatus status = g_io_channel_read_chars(GlobalEvents::channel,
						   buffer, sizeof(buffer),
						   &bytes_read, &error);
	if (status == G_IO_STATUS_ERROR)
		MPD_ERROR("error reading from pipe: %s", error->message);

	bool events[GlobalEvents::MAX];
	GlobalEvents::mutex.lock();
	memcpy(events, GlobalEvents::flags, sizeof(events));
	memset(GlobalEvents::flags, 0,
	       sizeof(GlobalEvents::flags));
	GlobalEvents::mutex.unlock();

	for (unsigned i = 0; i < GlobalEvents::MAX; ++i)
		if (events[i])
			/* invoke the event handler */
			InvokeGlobalEvent(GlobalEvents::Event(i));

	return true;
}

void
GlobalEvents::Initialize()
{
	if (pipe_cloexec_nonblock(fds) < 0)
		MPD_ERROR("Couldn't open pipe: %s", strerror(errno));

#ifndef G_OS_WIN32
	channel = g_io_channel_unix_new(fds[0]);
#else
	channel = g_io_channel_win32_new_fd(fds[0]);
#endif
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, false);

	source_id = g_io_add_watch(channel, G_IO_IN,
				   GlobalEventCallback, NULL);
}

void
GlobalEvents::Deinitialize()
{
	g_source_remove(source_id);
	g_io_channel_unref(channel);

#ifndef WIN32
	/* By some strange reason this call hangs on Win32 */
	close(fds[0]);
#endif
	close(fds[1]);
}

void
GlobalEvents::Register(Event event, Handler callback)
{
	assert((unsigned)event < MAX);
	assert(handlers[event] == NULL);

	handlers[event] = callback;
}

void
GlobalEvents::Emit(Event event)
{
	assert((unsigned)event < MAX);

	mutex.lock();
	if (flags[event]) {
		/* already set: don't write */
		mutex.unlock();
		return;
	}

	flags[event] = true;
	mutex.unlock();

	ssize_t w = write(fds[1], "", 1);
	if (w < 0 && errno != EAGAIN && errno != EINTR)
		MPD_ERROR("error writing to pipe: %s", strerror(errno));
}

void
GlobalEvents::FastEmit(Event event)
{
	assert((unsigned)event < MAX);

	flags[event] = true;

	G_GNUC_UNUSED ssize_t nbytes = write(fds[1], "", 1);
}
