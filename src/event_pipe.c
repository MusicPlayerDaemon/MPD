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

#include "config.h"
#include "event_pipe.h"
#include "fd_util.h"
#include "mpd_error.h"

#include <stdbool.h>
#include <assert.h>
#include <glib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef WIN32
/* for _O_BINARY */
#include <fcntl.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "event_pipe"

static int event_pipe[2];
static GIOChannel *event_channel;
static guint event_pipe_source_id;
static GMutex *event_pipe_mutex;
static bool pipe_events[PIPE_EVENT_MAX];
static event_pipe_callback_t event_pipe_callbacks[PIPE_EVENT_MAX];

/**
 * Invoke the callback for a certain event.
 */
static void
event_pipe_invoke(enum pipe_event event)
{
	assert((unsigned)event < PIPE_EVENT_MAX);
	assert(event_pipe_callbacks[event] != NULL);

	event_pipe_callbacks[event]();
}

static gboolean
main_notify_event(G_GNUC_UNUSED GIOChannel *source,
		  G_GNUC_UNUSED GIOCondition condition,
		  G_GNUC_UNUSED gpointer data)
{
	char buffer[256];
	gsize bytes_read;
	GError *error = NULL;
	GIOStatus status = g_io_channel_read_chars(event_channel,
						   buffer, sizeof(buffer),
						   &bytes_read, &error);
	if (status == G_IO_STATUS_ERROR)
		MPD_ERROR("error reading from pipe: %s", error->message);

	bool events[PIPE_EVENT_MAX];
	g_mutex_lock(event_pipe_mutex);
	memcpy(events, pipe_events, sizeof(events));
	memset(pipe_events, 0, sizeof(pipe_events));
	g_mutex_unlock(event_pipe_mutex);

	for (unsigned i = 0; i < PIPE_EVENT_MAX; ++i)
		if (events[i])
			/* invoke the event handler */
			event_pipe_invoke(i);

	return true;
}

void event_pipe_init(void)
{
	GIOChannel *channel;
	int ret;

	ret = pipe_cloexec_nonblock(event_pipe);
	if (ret < 0)
		MPD_ERROR("Couldn't open pipe: %s", strerror(errno));

#ifndef G_OS_WIN32
	channel = g_io_channel_unix_new(event_pipe[0]);
#else
	channel = g_io_channel_win32_new_fd(event_pipe[0]);
#endif
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, false);

	event_pipe_source_id = g_io_add_watch(channel, G_IO_IN,
					      main_notify_event, NULL);

	event_channel = channel;

	event_pipe_mutex = g_mutex_new();
}

void event_pipe_deinit(void)
{
	g_mutex_free(event_pipe_mutex);

	g_source_remove(event_pipe_source_id);
	g_io_channel_unref(event_channel);

#ifndef WIN32
	/* By some strange reason this call hangs on Win32 */
	close(event_pipe[0]);
#endif
	close(event_pipe[1]);
}

void
event_pipe_register(enum pipe_event event, event_pipe_callback_t callback)
{
	assert((unsigned)event < PIPE_EVENT_MAX);
	assert(event_pipe_callbacks[event] == NULL);

	event_pipe_callbacks[event] = callback;
}

void event_pipe_emit(enum pipe_event event)
{
	ssize_t w;

	assert((unsigned)event < PIPE_EVENT_MAX);

	g_mutex_lock(event_pipe_mutex);
	if (pipe_events[event]) {
		/* already set: don't write */
		g_mutex_unlock(event_pipe_mutex);
		return;
	}

	pipe_events[event] = true;
	g_mutex_unlock(event_pipe_mutex);

	w = write(event_pipe[1], "", 1);
	if (w < 0 && errno != EAGAIN && errno != EINTR)
		MPD_ERROR("error writing to pipe: %s", strerror(errno));
}

void event_pipe_emit_fast(enum pipe_event event)
{
	assert((unsigned)event < PIPE_EVENT_MAX);

	pipe_events[event] = true;

	G_GNUC_UNUSED ssize_t nbytes = write(event_pipe[1], "", 1);
}
