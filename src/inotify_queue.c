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
#include "inotify_queue.h"
#include "update.h"

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

static GSList *inotify_queue;
static guint queue_source_id;

void
mpd_inotify_queue_init(void)
{
}

static void
free_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	g_free(data);
}

void
mpd_inotify_queue_finish(void)
{
	if (queue_source_id != 0)
		g_source_remove(queue_source_id);

	g_slist_foreach(inotify_queue, free_callback, NULL);
	g_slist_free(inotify_queue);
}

static gboolean
mpd_inotify_run_update(G_GNUC_UNUSED gpointer data)
{
	unsigned id;

	while (inotify_queue != NULL) {
		char *uri_utf8 = inotify_queue->data;

		id = update_enqueue(uri_utf8, false);
		if (id == 0)
			/* retry later */
			return true;

		g_debug("updating '%s' job=%u", uri_utf8, id);

		g_free(uri_utf8);
		inotify_queue = g_slist_delete_link(inotify_queue,
						    inotify_queue);
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
mpd_inotify_enqueue(char *uri_utf8)
{
	GSList *old_queue = inotify_queue;

	if (queue_source_id != 0)
		g_source_remove(queue_source_id);
	queue_source_id = g_timeout_add_seconds(INOTIFY_UPDATE_DELAY_S,
					mpd_inotify_run_update, NULL);

	inotify_queue = NULL;
	while (old_queue != NULL) {
		char *current_uri = old_queue->data;

		if (path_in(uri_utf8, current_uri)) {
			/* already enqueued */
			g_free(uri_utf8);
			inotify_queue = g_slist_concat(inotify_queue,
						       old_queue);
			return;
		}

		old_queue = g_slist_delete_link(old_queue, old_queue);

		if (path_in(current_uri, uri_utf8))
			/* existing path is a sub-path of the new
			   path; we can dequeue the existing path and
			   update the new path instead */
			g_free(current_uri);
		else
			/* move the existing path to the new queue */
			inotify_queue = g_slist_prepend(inotify_queue,
							current_uri);
	}

	inotify_queue = g_slist_prepend(inotify_queue, uri_utf8);
}
