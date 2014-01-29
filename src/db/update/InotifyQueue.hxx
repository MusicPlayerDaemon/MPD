/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "event/TimeoutMonitor.hxx"
#include "Compiler.h"

#include <list>
#include <string>

class UpdateService;

class InotifyQueue final : private TimeoutMonitor {
	UpdateService &update;

	std::list<std::string> queue;

public:
	InotifyQueue(EventLoop &_loop, UpdateService &_update)
		:TimeoutMonitor(_loop), update(_update) {}

	void Enqueue(const char *uri_utf8);

private:
	virtual void OnTimeout() override;
};

#endif
