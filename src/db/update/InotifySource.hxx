/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_INOTIFY_SOURCE_HXX
#define MPD_INOTIFY_SOURCE_HXX

#include "event/PipeEvent.hxx"

typedef void (*mpd_inotify_callback_t)(int wd, unsigned mask,
				       const char *name, void *ctx);

class InotifySource final {
	PipeEvent socket_event;

	mpd_inotify_callback_t callback;
	void *callback_ctx;

public:
	/**
	 * Creates a new inotify source and registers it in the
	 * #EventLoop.
	 *
	 * Throws #std::system_error on error.
	 *
	 * @param callback a callback invoked for events received from
	 * the kernel
	 */
	InotifySource(EventLoop &_loop,
		      mpd_inotify_callback_t callback, void *ctx);

	~InotifySource() noexcept {
		socket_event.Close();
	}

	/**
	 * Adds a path to the notify list.
	 *
	 * Throws #std::system_error on error.
	 *
	 * @return a watch descriptor
	 */
	int Add(const char *path_fs, unsigned mask);

	/**
	 * Removes a path from the notify list.
	 *
	 * @param wd the watch descriptor returned by mpd_inotify_source_add()
	 */
	void Remove(unsigned wd) noexcept;

private:
	void OnSocketReady(unsigned flags) noexcept;
};

#endif
