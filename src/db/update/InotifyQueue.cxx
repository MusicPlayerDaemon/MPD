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

#include "config.h"
#include "InotifyQueue.hxx"
#include "InotifyDomain.hxx"
#include "Service.hxx"
#include "Log.hxx"

#include <string.h>

/**
 * Wait this long after the last change before calling
 * update_enqueue().  This increases the probability that updates can
 * be bundled.
 */
static constexpr unsigned INOTIFY_UPDATE_DELAY_S = 5;

void
InotifyQueue::OnTimeout()
{
	unsigned id;

	while (!queue.empty()) {
		const char *uri_utf8 = queue.front().c_str();

		id = update.Enqueue(uri_utf8, false);
		if (id == 0) {
			/* retry later */
			ScheduleSeconds(INOTIFY_UPDATE_DELAY_S);
			return;
		}

		FormatDebug(inotify_domain, "updating '%s' job=%u",
			    uri_utf8, id);

		queue.pop_front();
	}
}

static bool
path_in(const char *path, const char *possible_parent)
{
	size_t length = strlen(possible_parent);

	return path[0] == 0 ||
		(memcmp(possible_parent, path, length) == 0 &&
		 (path[length] == 0 || path[length] == '/'));
}

void
InotifyQueue::Enqueue(const char *uri_utf8)
{
	ScheduleSeconds(INOTIFY_UPDATE_DELAY_S);

	for (auto i = queue.begin(), end = queue.end(); i != end;) {
		const char *current_uri = i->c_str();

		if (path_in(uri_utf8, current_uri))
			/* already enqueued */
			return;

		if (path_in(current_uri, uri_utf8))
			/* existing path is a sub-path of the new
			   path; we can dequeue the existing path and
			   update the new path instead */
			i = queue.erase(i);
		else
			++i;
	}

	queue.emplace_back(uri_utf8);
}
