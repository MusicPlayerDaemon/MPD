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

#include "Service.hxx"
#include "Walk.hxx"
#include "UpdateDomain.hxx"
#include "db/DatabaseListener.hxx"
#include "db/DatabaseLock.hxx"
#include "db/plugins/simple/SimpleDatabasePlugin.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "storage/CompositeStorage.hxx"
#include "protocol/Ack.hxx"
#include "Idle.hxx"
#include "Log.hxx"
#include "thread/Thread.hxx"
#include "thread/Name.hxx"
#include "thread/Util.hxx"

#ifndef NDEBUG
#include "event/Loop.hxx"
#endif

#include <cassert>

UpdateService::UpdateService(const ConfigData &_config,
			     EventLoop &_loop, SimpleDatabase &_db,
			     CompositeStorage &_storage,
			     DatabaseListener &_listener) noexcept
	:config(_config),
	 defer(_loop, BIND_THIS_METHOD(RunDeferred)),
	 db(_db), storage(_storage),
	 listener(_listener),
	 update_thread(BIND_THIS_METHOD(Task))
{
}

UpdateService::~UpdateService() noexcept
{
	CancelAllAsync();

	if (update_thread.IsDefined())
		update_thread.Join();
}

void
UpdateService::CancelAllAsync() noexcept
{
	assert(GetEventLoop().IsInside());

	queue.Clear();

	if (walk != nullptr)
		walk->Cancel();
}

void
UpdateService::CancelMount(const char *uri) noexcept
{
	/* determine which (mounted) database will be updated and what
	   storage will be scanned */

	Directory::LookupResult lr;
	{
		const ScopeDatabaseLock protect;
		lr = db.GetRoot().LookupDirectory(uri);
	}

	if (!lr.directory->IsMount())
		return;

	bool cancel_current = false;

	Storage *storage2 = storage.GetMount(uri);
	if (storage2 != nullptr) {
		queue.Erase(*storage2);
		cancel_current = next.IsDefined() && next.storage == storage2;
	}

	if (auto *db2 = dynamic_cast<SimpleDatabase *>(lr.directory->mounted_database.get())) {
		queue.Erase(*db2);
		cancel_current |= next.IsDefined() && next.db == db2;
	}

	if (cancel_current && walk != nullptr) {
		walk->Cancel();

		if (update_thread.IsDefined())
			update_thread.Join();
	}
}

inline void
UpdateService::Task() noexcept
{
	assert(walk != nullptr);

	SetThreadName("update");

	if (!next.path_utf8.empty())
		FmtDebug(update_domain, "starting: {}", next.path_utf8);
	else
		LogDebug(update_domain, "starting");

	SetThreadIdlePriority();

	modified = walk->Walk(next.db->GetRoot(), next.path_utf8.c_str(),
			      next.discard);

	if (modified || !next.db->FileExists()) {
		try {
			next.db->Save();
		} catch (...) {
			LogError(std::current_exception(),
				 "Failed to save database");
		}
	}

	if (!next.path_utf8.empty())
		FmtDebug(update_domain, "finished: {}", next.path_utf8);
	else
		LogDebug(update_domain, "finished");

	defer.Schedule();
}

void
UpdateService::StartThread(UpdateQueueItem &&i)
{
	assert(GetEventLoop().IsInside());
	assert(walk == nullptr);

	modified = false;

	next = std::move(i);
	walk = std::make_unique<UpdateWalk>(config, GetEventLoop(), listener,
					    *next.storage);

	update_thread.Start();

	FmtDebug(update_domain,
		 "spawned thread for update job id {}", next.id);
}

unsigned
UpdateService::GenerateId() noexcept
{
	unsigned id = update_task_id + 1;
	if (id > update_task_id_max)
		id = 1;
	return id;
}

unsigned
UpdateService::Enqueue(std::string_view path, bool discard)
{
	assert(GetEventLoop().IsInside());

	/* determine which (mounted) database will be updated and what
	   storage will be scanned */
	SimpleDatabase *db2;
	Storage *storage2;

	Directory::LookupResult lr;
	{
		const ScopeDatabaseLock protect;
		lr = db.GetRoot().LookupDirectory(path);
	}

	if (lr.directory->IsMount()) {
		/* follow the mountpoint, update the mounted
		   database */

		db2 = dynamic_cast<SimpleDatabase *>(lr.directory->mounted_database.get());
		if (db2 == nullptr)
			throw std::runtime_error("Cannot update this type of database");

		if (lr.rest.data() == nullptr) {
			storage2 = storage.GetMount(path);
			path = "";
		} else {
			storage2 = storage.GetMount(lr.uri);
			path = lr.rest;
		}
	} else {
		/* use the "root" database/storage */

		db2 = &db;
		storage2 = storage.GetMount("");
	}

	if (storage2 == nullptr)
		/* no storage found at this mount point - should not
		   happen */
		throw std::runtime_error("No storage at this path");

	if (walk != nullptr) {
		const unsigned id = GenerateId();
		if (!queue.Push(*db2, *storage2, path, discard, id))
			throw ProtocolError(ACK_ERROR_UPDATE_ALREADY,
					    "Update queue is full");

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
UpdateService::RunDeferred() noexcept
{
	assert(next.IsDefined());
	assert(walk != nullptr);

	/* wait for thread to finish only if it wasn't cancelled by
	   CancelMount() */
	if (update_thread.IsDefined())
		update_thread.Join();

	walk.reset();

	next.Clear();

	idle_add(IDLE_UPDATE);

	if (modified)
		/* send "idle" events */
		listener.OnDatabaseModified();

	auto i = queue.Pop();
	if (i.IsDefined()) {
		/* schedule the next path */
		StartThread(std::move(i));
	}
}
