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
#include "InotifyQueue.hxx"
#include "UpdateGlue.hxx"

#include <list>
#include <string>

#include <glib.h>

#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "inotify"

enum {
	/**
	 * Wait this long after the last change before calling
	 * update_enqueue().  This increases the probability that
	 * updates can be bundled.
	 */
	INOTIFY_UPDATE_DELAY_S = 5,
};

static std::list<std::string> inotify_queue;
static guint queue_source_id;

void
mpd_inotify_queue_init(void)
{
}

void
mpd_inotify_queue_finish(void)
{
	if (queue_source_id != 0)
		g_source_remove(queue_source_id);
}

static gboolean
mpd_inotify_run_update(G_GNUC_UNUSED gpointer data)
{
	unsigned id;

	while (!inotify_queue.empty()) {
		const char *uri_utf8 = inotify_queue.front().c_str();

		id = update_enqueue(uri_utf8, false);
		if (id == 0)
			/* retry later */
			return true;

		g_debug("updating '%s' job=%u", uri_utf8, id);

		inotify_queue.pop_front();
	}

	/* done, remove the timer event by returning false */
	queue_source_id = 0;
	return false;
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
mpd_inotify_enqueue(const char *uri_utf8)
{
	if (queue_source_id != 0)
		g_source_remove(queue_source_id);
	queue_source_id = g_timeout_add_seconds(INOTIFY_UPDATE_DELAY_S,
					mpd_inotify_run_update, NULL);

	for (auto i = inotify_queue.begin(), end = inotify_queue.end();
	     i != end;) {
		const char *current_uri = i->c_str();

		if (path_in(uri_utf8, current_uri))
			/* already enqueued */
			return;

		if (path_in(current_uri, uri_utf8))
			/* existing path is a sub-path of the new
			   path; we can dequeue the existing path and
			   update the new path instead */
			i = inotify_queue.erase(i);
		else
			++i;
	}

	inotify_queue.emplace_back(uri_utf8);
}
