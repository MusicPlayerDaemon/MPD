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

#ifndef MPD_INOTIFY_QUEUE_HXX
#define MPD_INOTIFY_QUEUE_HXX

#include "event/CoarseTimerEvent.hxx"

#include <list>
#include <string>

class UpdateService;

class InotifyQueue final {
	UpdateService &update;

	std::list<std::string> queue;

	CoarseTimerEvent delay_event;

public:
	InotifyQueue(EventLoop &_loop, UpdateService &_update) noexcept
		:update(_update),
		 delay_event(_loop, BIND_THIS_METHOD(OnDelay)) {}

	void Enqueue(const char *uri_utf8) noexcept;

private:
	void OnDelay() noexcept;
};

#endif
