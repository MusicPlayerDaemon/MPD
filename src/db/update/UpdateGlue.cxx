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
#include "UpdateGlue.hxx"
#include "UpdateQueue.hxx"
#include "UpdateWalk.hxx"
#include "UpdateRemove.hxx"
#include "UpdateDomain.hxx"
#include "Mapper.hxx"
#include "db/DatabaseSimple.hxx"
#include "Idle.hxx"
#include "GlobalEvents.hxx"
#include "util/Error.hxx"
#include "Log.hxx"
#include "Main.hxx"
#include "Instance.hxx"
#include "system/FatalError.hxx"
#include "thread/Id.hxx"
#include "thread/Thread.hxx"
#include "thread/Util.hxx"

#include <assert.h>

static enum update_progress {
	UPDATE_PROGRESS_IDLE = 0,
	UPDATE_PROGRESS_RUNNING = 1,
	UPDATE_PROGRESS_DONE = 2
} progress;

static bool modified;

static Thread update_thread;

static const unsigned update_task_id_max = 1 << 15;

static unsigned update_task_id;

static UpdateQueueItem next;

unsigned
isUpdatingDB(void)
{
	return next.id;
}

static void
update_task(gcc_unused void *ctx)
{
	if (!next.path_utf8.empty())
		FormatDebug(update_domain, "starting: %s",
			    next.path_utf8.c_str());
	else
		LogDebug(update_domain, "starting");

	SetThreadIdlePriority();

	modified = update_walk(next.path_utf8.c_str(), next.discard);

	if (modified || !db_exists()) {
		Error error;
		if (!db_save(error))
			LogError(error, "Failed to save database");
	}

	if (!next.path_utf8.empty())
		FormatDebug(update_domain, "finished: %s",
			    next.path_utf8.c_str());
	else
		LogDebug(update_domain, "finished");

	progress = UPDATE_PROGRESS_DONE;
	GlobalEvents::Emit(GlobalEvents::UPDATE);
}

static void
spawn_update_task(UpdateQueueItem &&i)
{
	assert(main_thread.IsInside());

	progress = UPDATE_PROGRESS_RUNNING;
	modified = false;

	next = std::move(i);

	Error error;
	if (!update_thread.Start(update_task, nullptr, error))
		FatalError(error);

	FormatDebug(update_domain,
		    "spawned thread for update job id %i", next.id);
}

static unsigned
generate_update_id()
{
	unsigned id = update_task_id + 1;
	if (id > update_task_id_max)
		id = 1;
	return id;
}

unsigned
update_enqueue(const char *path, bool discard)
{
	assert(main_thread.IsInside());

	if (!db_is_simple() || !mapper_has_music_directory())
		return 0;

	if (progress != UPDATE_PROGRESS_IDLE) {
		const unsigned id = generate_update_id();
		if (!update_queue_push(path, discard, id))
			return 0;

		update_task_id = id;
		return id;
	}

	const unsigned id = update_task_id = generate_update_id();
	spawn_update_task(UpdateQueueItem(path, discard, id));

	idle_add(IDLE_UPDATE);

	return id;
}

/**
 * Called in the main thread after the database update is finished.
 */
static void update_finished_event(void)
{
	assert(progress == UPDATE_PROGRESS_DONE);
	assert(next.IsDefined());

	update_thread.Join();
	next = UpdateQueueItem();

	idle_add(IDLE_UPDATE);

	if (modified)
		/* send "idle" events */
		instance->DatabaseModified();

	auto i = update_queue_shift();
	if (i.IsDefined()) {
		/* schedule the next path */
		spawn_update_task(std::move(i));
	} else {
		progress = UPDATE_PROGRESS_IDLE;
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
