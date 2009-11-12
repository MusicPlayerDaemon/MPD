/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "inotify_source.h"
#include "fifo_buffer.h"
#include "fd_util.h"

#include <sys/inotify.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "inotify"

struct mpd_inotify_source {
	int fd;

	GIOChannel *channel;

	/**
	 * The channel's source id in the GLib main loop.
	 */
	guint id;

	struct fifo_buffer *buffer;

	mpd_inotify_callback_t callback;
	void *callback_ctx;
};

/**
 * A GQuark for GError instances.
 */
static inline GQuark
mpd_inotify_quark(void)
{
	return g_quark_from_static_string("inotify");
}

static gboolean
mpd_inotify_in_event(G_GNUC_UNUSED GIOChannel *_source,
		     G_GNUC_UNUSED GIOCondition condition,
		     gpointer data)
{
	struct mpd_inotify_source *source = data;
	void *dest;
	size_t length;
	ssize_t nbytes;
	const struct inotify_event *event;

	dest = fifo_buffer_write(source->buffer, &length);
	if (dest == NULL)
		g_error("buffer full");

	nbytes = read(source->fd, dest, length);
	if (nbytes < 0)
		g_error("failed to read from inotify: %s", g_strerror(errno));
	if (nbytes == 0)
		g_error("end of file from inotify");

	fifo_buffer_append(source->buffer, nbytes);

	while (true) {
		const char *name;

		event = fifo_buffer_read(source->buffer, &length);
		if (event == NULL || length < sizeof(*event) ||
		    length < sizeof(*event) + event->len)
			break;

		if (event->len > 0 && event->name[event->len - 1] == 0)
			name = event->name;
		else
			name = NULL;

		source->callback(event->wd, event->mask, name,
				 source->callback_ctx);
		fifo_buffer_consume(source->buffer,
				    sizeof(*event) + event->len);
	}

	return true;
}

struct mpd_inotify_source *
mpd_inotify_source_new(mpd_inotify_callback_t callback, void *callback_ctx,
		       GError **error_r)
{
	struct mpd_inotify_source *source =
		g_new(struct mpd_inotify_source, 1);

	source->fd = inotify_init_cloexec();
	if (source->fd < 0) {
		g_set_error(error_r, mpd_inotify_quark(), errno,
			    "inotify_init() has failed: %s",
			    g_strerror(errno));
		g_free(source);
		return NULL;
	}

	source->buffer = fifo_buffer_new(4096);

	source->channel = g_io_channel_unix_new(source->fd);
	source->id = g_io_add_watch(source->channel, G_IO_IN,
				    mpd_inotify_in_event, source);

	source->callback = callback;
	source->callback_ctx = callback_ctx;

	return source;
}

void
mpd_inotify_source_free(struct mpd_inotify_source *source)
{
	g_source_remove(source->id);
	g_io_channel_unref(source->channel);
	fifo_buffer_free(source->buffer);
	close(source->fd);
	g_free(source);
}

int
mpd_inotify_source_add(struct mpd_inotify_source *source,
		       const char *path_fs, unsigned mask,
		       GError **error_r)
{
	int wd = inotify_add_watch(source->fd, path_fs, mask);
	if (wd < 0)
		g_set_error(error_r, mpd_inotify_quark(), errno,
			    "inotify_add_watch() has failed: %s",
			    g_strerror(errno));

	return wd;
}

void
mpd_inotify_source_rm(struct mpd_inotify_source *source, unsigned wd)
{
	int ret = inotify_rm_watch(source->fd, wd);
	if (ret < 0 && errno != EINVAL)
		g_warning("inotify_rm_watch() has failed: %s",
			  g_strerror(errno));

	/* EINVAL may happen here when the file has been deleted; the
	   kernel seems to auto-unregister deleted files */
}
