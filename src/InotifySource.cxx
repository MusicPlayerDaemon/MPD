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
#include "InotifySource.hxx"
#include "util/fifo_buffer.h"
#include "fd_util.h"
#include "FatalError.hxx"

#include <glib.h>

#include <sys/inotify.h>
#include <unistd.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "inotify"

/**
 * A GQuark for GError instances.
 */
static inline GQuark
mpd_inotify_quark(void)
{
	return g_quark_from_static_string("inotify");
}

bool
InotifySource::OnSocketReady(gcc_unused unsigned flags)
{
	void *dest;
	size_t length;
	ssize_t nbytes;

	dest = fifo_buffer_write(buffer, &length);
	if (dest == NULL)
		FatalError("buffer full");

	nbytes = read(Get(), dest, length);
	if (nbytes < 0)
		FatalSystemError("Failed to read from inotify");
	if (nbytes == 0)
		FatalError("end of file from inotify");

	fifo_buffer_append(buffer, nbytes);

	while (true) {
		const char *name;

		const struct inotify_event *event =
			(const struct inotify_event *)
			fifo_buffer_read(buffer, &length);
		if (event == NULL || length < sizeof(*event) ||
		    length < sizeof(*event) + event->len)
			break;

		if (event->len > 0 && event->name[event->len - 1] == 0)
			name = event->name;
		else
			name = NULL;

		callback(event->wd, event->mask, name, callback_ctx);
		fifo_buffer_consume(buffer, sizeof(*event) + event->len);
	}

	return true;
}

inline
InotifySource::InotifySource(EventLoop &_loop,
			     mpd_inotify_callback_t _callback, void *_ctx,
			     int _fd)
	:SocketMonitor(_fd, _loop),
	 callback(_callback), callback_ctx(_ctx),
	 buffer(fifo_buffer_new(4096))
{
	ScheduleRead();

}

InotifySource *
InotifySource::Create(EventLoop &loop,
		      mpd_inotify_callback_t callback, void *callback_ctx,
		      GError **error_r)
{
	int fd = inotify_init_cloexec();
	if (fd < 0) {
		g_set_error(error_r, mpd_inotify_quark(), errno,
			    "inotify_init() has failed: %s",
			    g_strerror(errno));
		return NULL;
	}

	return new InotifySource(loop, callback, callback_ctx, fd);
}

InotifySource::~InotifySource()
{
	fifo_buffer_free(buffer);
}

int
InotifySource::Add(const char *path_fs, unsigned mask, GError **error_r)
{
	int wd = inotify_add_watch(Get(), path_fs, mask);
	if (wd < 0)
		g_set_error(error_r, mpd_inotify_quark(), errno,
			    "inotify_add_watch() has failed: %s",
			    g_strerror(errno));

	return wd;
}

void
InotifySource::Remove(unsigned wd)
{
	int ret = inotify_rm_watch(Get(), wd);
	if (ret < 0 && errno != EINVAL)
		g_warning("inotify_rm_watch() has failed: %s",
			  g_strerror(errno));

	/* EINVAL may happen here when the file has been deleted; the
	   kernel seems to auto-unregister deleted files */
}
