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
#include "Service.hxx"
#include "Walk.hxx"
#include "UpdateDomain.hxx"
#include "db/DatabaseListener.hxx"
#include "db/DatabaseLock.hxx"
#include "db/plugins/simple/SimpleDatabasePlugin.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "storage/CompositeStorage.hxx"
#include "Idle.hxx"
#include "util/Error.hxx"
#include "Log.hxx"
#include "Instance.hxx"
#include "system/FatalError.hxx"
#include "thread/Id.hxx"
#include "thread/Thread.hxx"
#include "thread/Util.hxx"

#ifndef NDEBUG
#include "event/Loop.hxx"
#endif

#include <assert.h>

UpdateService::UpdateService(EventLoop &_loop, SimpleDatabase &_db,
			     CompositeStorage &_storage,
			     DatabaseListener &_listener)
	:DeferredMonitor(_loop),
	 db(_db), storage(_storage),
	 listener(_listener),
	 progress(UPDATE_PROGRESS_IDLE),
	 update_task_id(0),
	 walk(nullptr)
{
}

UpdateService::~UpdateService()
{
	CancelAllAsync();

	if (update_thread.IsDefined())
		update_thread.Join();

	delete walk;
}

void
UpdateService::CancelAllAsync()
{
	assert(GetEventLoop().IsInsideOrNull());

	queue.Clear();

	if (walk != nullptr)
		walk->Cancel();
}

void
UpdateService::CancelMount(const char *uri)
{
	/* determine which (mounted) database will be updated and what
	   storage will be scanned */

	db_lock();
	const auto lr = db.GetRoot().LookupDirectory(uri);
	db_unlock();

	if (!lr.directory->IsMount())
		return;

	bool cancel_current = false;

	Storage *storage2 = storage.GetMount(uri);
	if (storage2 != nullptr) {
		queue.Erase(*storage2);
		cancel_current = next.IsDefined() && next.storage == storage2;
	}

	Database &_db2 = *lr.directory->mounted_database;
	if (_db2.IsPlugin(simple_db_plugin)) {
		SimpleDatabase &db2 = static_cast<SimpleDatabase &>(_db2);
		queue.Erase(db2);
		cancel_current |= next.IsDefined() && next.db == &db2;
	}

	if (cancel_current && walk != nullptr) {
		walk->Cancel();

		if (update_thread.IsDefined())
			update_thread.Join();
	}
}

inline void
UpdateService::Task()
{
	assert(walk != nullptr);

	if (!next.path_utf8.empty())
		FormatDebug(update_domain, "starting: %s",
			    next.path_utf8.c_str());
	else
		LogDebug(update_domain, "starting");

	SetThreadIdlePriority();

	modified = walk->Walk(next.db->GetRoot(), next.path_utf8.c_str(),
			      next.discard);

	if (modified || !next.db->FileExists()) {
		Error error;
		if (!next.db->Save(error))
			LogError(error, "Failed to save database");
	}

	if (!next.path_utf8.empty())
		FormatDebug(update_domain, "finished: %s",
			    next.path_utf8.c_str());
	else
		LogDebug(update_domain, "finished");

	progress = UPDATE_PROGRESS_DONE;
	DeferredMonitor::Schedule();
}

void
UpdateService::Task(void *ctx)
{
	UpdateService &service = *(UpdateService *)ctx;
	return service.Task();
}

void
UpdateService::StartThread(UpdateQueueItem &&i)
{
	assert(GetEventLoop().IsInsideOrNull());
	assert(walk == nullptr);

	progress = UPDATE_PROGRESS_RUNNING;
	modified = false;

	next = std::move(i);
	walk = new UpdateWalk(GetEventLoop(), listener, *next.storage);

	Error error;
	if (!update_thread.Start(Task, this, error))
		FatalError(error);

	FormatDebug(update_domain,
		    "spawned thread for update job id %i", next.id);
}

unsigned
UpdateService::GenerateId()
{
	unsigned id = update_task_id + 1;
	if (id > update_task_id_max)
		id = 1;
	return id;
}

unsigned
UpdateService::Enqueue(const char *path, bool discard)
{
	assert(GetEventLoop().IsInsideOrNull());

	/* determine which (mounted) database will be updated and what
	   storage will be scanned */
	SimpleDatabase *db2;
	Storage *storage2;

	db_lock();
	const auto lr = db.GetRoot().LookupDirectory(path);
	db_unlock();
	if (lr.directory->IsMount()) {
		/* follow the mountpoint, update the mounted
		   database */

		Database &_db2 = *lr.directory->mounted_database;
		if (!_db2.IsPlugin(simple_db_plugin))
			/* cannot update this type of database */
			return 0;

		db2 = static_cast<SimpleDatabase *>(&_db2);

		if (lr.uri == nullptr) {
			storage2 = storage.GetMount(path);
			path = "";
		} else {
			assert(lr.uri > path);
			assert(lr.uri < path + strlen(path));
			assert(lr.uri[-1] == '/');

			const std::string mountpoint(path, lr.uri - 1);
			storage2 = storage.GetMount(mountpoint.c_str());
			path = lr.uri;
		}
	} else {
		/* use the "root" database/storage */

		db2 = &db;
		storage2 = storage.GetMount("");
	}

	if (storage2 == nullptr)
		/* no storage found at this mount point - should not
		   happen */
		return 0;

	if (progress != UPDATE_PROGRESS_IDLE) {
		const unsigned id = GenerateId();
		if (!queue.Push(*db2, *storage2, path, discard, id))
			return 0;

		update_task_id = id;
		return id;
	}

	const unsigned id = update_task_id = GenerateId();
	StartThread(UpdateQueueItem(*db2, *storage2, path, discard, id));

	idle_add(IDLE_UPDATE);

	return id;
}

/**
 * Called in the main thread after the database update is finished.
 */
void
UpdateService::RunDeferred()
{
	assert(progress == UPDATE_PROGRESS_DONE);
	assert(next.IsDefined());
	assert(walk != nullptr);

	/* wait for thread to finish only if it wasn't cancelled by
	   CancelMount() */
	if (update_thread.IsDefined())
		update_thread.Join();

	delete walk;
	walk = nullptr;

	next = UpdateQueueItem();

	idle_add(IDLE_UPDATE);

	if (modified)
		/* send "idle" events */
		listener.OnDatabaseModified();

	auto i = queue.Pop();
	if (i.IsDefined()) {
		/* schedule the next path */
		StartThread(std::move(i));
	} else {
		progress = UPDATE_PROGRESS_IDLE;
	}
}
