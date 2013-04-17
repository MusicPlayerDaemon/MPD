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
#include "UpdateGlue.hxx"
#include "UpdateQueue.hxx"
#include "UpdateWalk.hxx"
#include "UpdateRemove.hxx"
#include "Mapper.hxx"
#include "DatabaseSimple.hxx"
#include "Idle.hxx"
#include "GlobalEvents.hxx"

extern "C" {
#include "stats.h"
}

#include "Main.hxx"
#include "Instance.hxx"
#include "mpd_error.h"

#include <glib.h>

#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "update"

static enum update_progress {
	UPDATE_PROGRESS_IDLE = 0,
	UPDATE_PROGRESS_RUNNING = 1,
	UPDATE_PROGRESS_DONE = 2
} progress;

static bool modified;

static GThread *update_thr;

static const unsigned update_task_id_max = 1 << 15;

static unsigned update_task_id;

/* XXX this flag is passed to update_task() */
static bool discard;

unsigned
isUpdatingDB(void)
{
	return (progress != UPDATE_PROGRESS_IDLE) ? update_task_id : 0;
}

static void * update_task(void *_path)
{
	const char *path = (const char *)_path;

	if (path != NULL && *path != 0)
		g_debug("starting: %s", path);
	else
		g_debug("starting");

	modified = update_walk(path, discard);

	if (modified || !db_exists()) {
		GError *error = NULL;
		if (!db_save(&error)) {
			g_warning("Failed to save database: %s",
				  error->message);
			g_error_free(error);
		}
	}

	if (path != NULL && *path != 0)
		g_debug("finished: %s", path);
	else
		g_debug("finished");
	g_free(_path);

	progress = UPDATE_PROGRESS_DONE;
	GlobalEvents::Emit(GlobalEvents::UPDATE);
	return NULL;
}

static void
spawn_update_task(const char *path)
{
	assert(g_thread_self() == main_task);

	progress = UPDATE_PROGRESS_RUNNING;
	modified = false;

#if GLIB_CHECK_VERSION(2,32,0)
	update_thr = g_thread_new("updadte", update_task, g_strdup(path));
#else
	GError *e = NULL;
	update_thr = g_thread_create(update_task, g_strdup(path), TRUE, &e);
	if (update_thr == NULL)
		MPD_ERROR("Failed to spawn update task: %s", e->message);
#endif

	if (++update_task_id > update_task_id_max)
		update_task_id = 1;
	g_debug("spawned thread for update job id %i", update_task_id);
}

unsigned
update_enqueue(const char *path, bool _discard)
{
	assert(g_thread_self() == main_task);

	if (!db_is_simple() || !mapper_has_music_directory())
		return 0;

	if (progress != UPDATE_PROGRESS_IDLE) {
		unsigned next_task_id =
			update_queue_push(path, discard, update_task_id);
		if (next_task_id == 0)
			return 0;

		return next_task_id > update_task_id_max ?  1 : next_task_id;
	}

	discard = _discard;
	spawn_update_task(path);

	idle_add(IDLE_UPDATE);

	return update_task_id;
}

/**
 * Called in the main thread after the database update is finished.
 */
static void update_finished_event(void)
{
	char *path;

	assert(progress == UPDATE_PROGRESS_DONE);

	g_thread_join(update_thr);

	idle_add(IDLE_UPDATE);

	if (modified)
		/* send "idle" events */
		instance->DatabaseModified();

	path = update_queue_shift(&discard);
	if (path != NULL) {
		/* schedule the next path */
		spawn_update_task(path);
		g_free(path);
	} else {
		progress = UPDATE_PROGRESS_IDLE;

		stats_update();
	}
}

void update_global_init(void)
{
	GlobalEvents::Register(GlobalEvents::UPDATE, update_finished_event);

	update_remove_global_init();
	update_walk_global_init();
}

void update_global_finish(void)
{
	update_walk_global_finish();
}
