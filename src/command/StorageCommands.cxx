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

#include "config.h"
#include "StorageCommands.hxx"
#include "Request.hxx"
#include "time/ChronoUtil.hxx"
#include "util/UriUtil.hxx"
#include "util/ConstBuffer.hxx"
#include "fs/Traits.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Instance.hxx"
#include "storage/Registry.hxx"
#include "storage/CompositeStorage.hxx"
#include "storage/FileInfo.hxx"
#include "db/plugins/simple/SimpleDatabasePlugin.hxx"
#include "db/update/Service.hxx"
#include "TimePrint.hxx"
#include "IdleFlags.hxx"

#include <fmt/format.h>

#include <memory>

gcc_pure
static bool
skip_path(const char *name_utf8) noexcept
{
	return std::strchr(name_utf8, '\n') != nullptr;
}

static void
handle_listfiles_storage(Response &r, StorageDirectoryReader &reader)
{
	const char *name_utf8;
	while ((name_utf8 = reader.Read()) != nullptr) {
		if (skip_path(name_utf8))
			continue;

		StorageFileInfo info;
		try {
			info = reader.GetInfo(false);
		} catch (...) {
			continue;
		}

		switch (info.type) {
		case StorageFileInfo::Type::OTHER:
			/* ignore */
			continue;

		case StorageFileInfo::Type::REGULAR:
			r.Fmt(FMT_STRING("file: {}\n"
					 "size: {}\n"),
			      name_utf8,
			      info.size);
			break;

		case StorageFileInfo::Type::DIRECTORY:
			r.Fmt(FMT_STRING("directory: {}\n"), name_utf8);
			break;
		}

		if (!IsNegative(info.mtime))
			time_print(r, "Last-Modified", info.mtime);
	}
}

#if defined(_WIN32) && GCC_CHECK_VERSION(4,6)
#pragma GCC diagnostic pop
#endif

CommandResult
handle_listfiles_storage(Response &r, Storage &storage, const char *uri)
{
	std::unique_ptr<StorageDirectoryReader> reader(storage.OpenDirectory(uri));
	handle_listfiles_storage(r, *reader);
	return CommandResult::OK;
}

CommandResult
handle_listfiles_storage(Client &client, Response &r, const char *uri)
{
	auto &event_loop = client.GetInstance().io_thread.GetEventLoop();
	std::unique_ptr<Storage> storage(CreateStorageURI(event_loop, uri));
	if (storage == nullptr) {
		r.Error(ACK_ERROR_ARG, "Unrecognized storage URI");
		return CommandResult::ERROR;
	}

	return handle_listfiles_storage(r, *storage, "");
}

static void
print_storage_uri(Client &client, Response &r, const Storage &storage)
{
	std::string uri = storage.MapUTF8("");
	if (uri.empty())
		return;

	if (PathTraitsUTF8::IsAbsolute(uri.c_str())) {
		/* storage points to local directory */

		if (!client.IsLocal())
			/* only "local" clients may see local paths
			   (same policy as with the "config"
			   command) */
			return;
	} else {
		/* hide username/passwords from client */

		std::string allocated = uri_remove_auth(uri.c_str());
		if (!allocated.empty())
			uri = std::move(allocated);
	}

	r.Fmt(FMT_STRING("storage: {}\n"), uri);
}

CommandResult
handle_listmounts(Client &client, [[maybe_unused]] Request args, Response &r)
{
	Storage *_composite = client.GetInstance().storage;
	if (_composite == nullptr) {
		r.Error(ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
	}

	CompositeStorage &composite = *(CompositeStorage *)_composite;

	const auto visitor = [&client, &r](const char *mount_uri,
					   const Storage &storage){
		r.Fmt(FMT_STRING("mount: {}\n"), mount_uri);
		print_storage_uri(client, r, storage);
	};

	composite.VisitMounts(visitor);

	return CommandResult::OK;
}

CommandResult
handle_mount(Client &client, Request args, Response &r)
{
	auto &instance = client.GetInstance();

	Storage *_composite = instance.storage;
	if (_composite == nullptr) {
		r.Error(ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
	}

	CompositeStorage &composite = *(CompositeStorage *)_composite;

	const char *const local_uri = args[0];
	const char *const remote_uri = args[1];

	if (*local_uri == 0) {
		r.Error(ACK_ERROR_ARG, "Bad mount point");
		return CommandResult::ERROR;
	}

	if (std::strchr(local_uri, '/') != nullptr) {
		/* allow only top-level mounts for now */
		/* TODO: eliminate this limitation after ensuring that
		   UpdateQueue::Erase() really gets called for every
		   unmount, and no Directory disappears recursively
		   during database update */
		r.Error(ACK_ERROR_ARG, "Bad mount point");
		return CommandResult::ERROR;
	}

	if (composite.IsMountPoint(local_uri)) {
		r.Error(ACK_ERROR_ARG, "Mount point busy");
		return CommandResult::ERROR;
	}

	if (composite.IsMounted(remote_uri)) {
		r.Error(ACK_ERROR_ARG, "This storage is already mounted");
		return CommandResult::ERROR;
	}

	auto &event_loop = instance.io_thread.GetEventLoop();
	auto storage = CreateStorageURI(event_loop, remote_uri);
	if (storage == nullptr) {
		r.Error(ACK_ERROR_ARG, "Unrecognized storage URI");
		return CommandResult::ERROR;
	}

	composite.Mount(local_uri, std::move(storage));
	instance.EmitIdle(IDLE_MOUNT);

#ifdef ENABLE_DATABASE
	if (auto *db = dynamic_cast<SimpleDatabase *>(instance.GetDatabase())) {
		bool need_update;

		try {
			need_update = !db->Mount(local_uri, remote_uri);
		} catch (...) {
			composite.Unmount(local_uri);
			throw;
		}

		// TODO: call Instance::OnDatabaseModified()?
		// TODO: trigger database update?
		instance.EmitIdle(IDLE_DATABASE);

		if (need_update) {
			UpdateService *update = client.GetInstance().update;
			if (update != nullptr)
				update->Enqueue(local_uri, false);
		}
	}
#endif

	return CommandResult::OK;
}

CommandResult
handle_unmount(Client &client, Request args, Response &r)
{
	auto &instance = client.GetInstance();

	Storage *_composite = instance.storage;
	if (_composite == nullptr) {
		r.Error(ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
	}

	CompositeStorage &composite = *(CompositeStorage *)_composite;

	const char *const local_uri = args.front();

	if (*local_uri == 0) {
		r.Error(ACK_ERROR_ARG, "Bad mount point");
		return CommandResult::ERROR;
	}

#ifdef ENABLE_DATABASE
	if (instance.update != nullptr)
		/* ensure that no database update will attempt to work
		   with the database/storage instances we're about to
		   destroy here */
		instance.update->CancelMount(local_uri);

	if (auto *db = dynamic_cast<SimpleDatabase *>(instance.GetDatabase())) {
		if (db->Unmount(local_uri))
			// TODO: call Instance::OnDatabaseModified()?
			instance.EmitIdle(IDLE_DATABASE);
	}
#endif

	if (!composite.Unmount(local_uri)) {
		r.Error(ACK_ERROR_ARG, "Not a mount point");
		return CommandResult::ERROR;
	}

	instance.EmitIdle(IDLE_MOUNT);

	return CommandResult::OK;
}
