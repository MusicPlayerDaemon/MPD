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

#define __STDC_FORMAT_MACROS /* for PRIu64 */

#include "config.h"
#include "StorageCommands.hxx"
#include "CommandError.hxx"
#include "protocol/Result.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "fs/Traits.hxx"
#include "client/Client.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "storage/Registry.hxx"
#include "storage/CompositeStorage.hxx"
#include "storage/FileInfo.hxx"
#include "db/plugins/simple/SimpleDatabasePlugin.hxx"
#include "db/update/Service.hxx"
#include "TimePrint.hxx"
#include "IOThread.hxx"
#include "Idle.hxx"

#include <inttypes.h> /* for PRIu64 */

gcc_pure
static bool
skip_path(const char *name_utf8)
{
	return strchr(name_utf8, '\n') != nullptr;
}

#if defined(WIN32) && GCC_CHECK_VERSION(4,6)
/* PRIu64 causes bogus compiler warning */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif

static bool
handle_listfiles_storage(Client &client, StorageDirectoryReader &reader,
			 Error &error)
{
	const char *name_utf8;
	while ((name_utf8 = reader.Read()) != nullptr) {
		if (skip_path(name_utf8))
			continue;

		FileInfo info;
		if (!reader.GetInfo(false, info, error))
			continue;

		switch (info.type) {
		case FileInfo::Type::OTHER:
			/* ignore */
			continue;

		case FileInfo::Type::REGULAR:
			client_printf(client, "file: %s\n"
				      "size: %" PRIu64 "\n",
				      name_utf8,
				      info.size);
			break;

		case FileInfo::Type::DIRECTORY:
			client_printf(client, "directory: %s\n", name_utf8);
			break;
		}

		if (info.mtime != 0)
			time_print(client, "Last-Modified", info.mtime);
	}

	return true;
}

#if defined(WIN32) && GCC_CHECK_VERSION(4,6)
#pragma GCC diagnostic pop
#endif

static bool
handle_listfiles_storage(Client &client, Storage &storage, const char *uri,
			 Error &error)
{
	auto reader = storage.OpenDirectory(uri, error);
	if (reader == nullptr)
		return false;

	bool success = handle_listfiles_storage(client, *reader, error);
	delete reader;
	return success;
}

CommandResult
handle_listfiles_storage(Client &client, Storage &storage, const char *uri)
{
	Error error;
	if (!handle_listfiles_storage(client, storage, uri, error))
		return print_error(client, error);

	return CommandResult::OK;
}

CommandResult
handle_listfiles_storage(Client &client, const char *uri)
{
	Error error;
	Storage *storage = CreateStorageURI(io_thread_get(), uri, error);
	if (storage == nullptr) {
		if (error.IsDefined())
			return print_error(client, error);

		command_error(client, ACK_ERROR_ARG,
			      "Unrecognized storage URI");
		return CommandResult::ERROR;
	}

	bool success = handle_listfiles_storage(client, *storage, "", error);
	delete storage;
	if (!success)
		return print_error(client, error);

	return CommandResult::OK;
}

static void
print_storage_uri(Client &client, const Storage &storage)
{
	std::string uri = storage.MapUTF8("");
	if (uri.empty())
		return;

	if (PathTraitsFS::IsAbsolute(uri.c_str())) {
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

	client_printf(client, "storage: %s\n", uri.c_str());
}

CommandResult
handle_listmounts(Client &client, gcc_unused unsigned argc, gcc_unused char *argv[])
{
	Storage *_composite = client.partition.instance.storage;
	if (_composite == nullptr) {
		command_error(client, ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
	}

	CompositeStorage &composite = *(CompositeStorage *)_composite;

	const auto visitor = [&client](const char *mount_uri,
				       const Storage &storage){
		client_printf(client, "mount: %s\n", mount_uri);
		print_storage_uri(client, storage);
	};

	composite.VisitMounts(visitor);

	return CommandResult::OK;
}

CommandResult
handle_mount(Client &client, gcc_unused unsigned argc, char *argv[])
{
	Storage *_composite = client.partition.instance.storage;
	if (_composite == nullptr) {
		command_error(client, ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
	}

	CompositeStorage &composite = *(CompositeStorage *)_composite;

	const char *const local_uri = argv[1];
	const char *const remote_uri = argv[2];

	if (*local_uri == 0) {
		command_error(client, ACK_ERROR_ARG, "Bad mount point");
		return CommandResult::ERROR;
	}

	if (strchr(local_uri, '/') != nullptr) {
		/* allow only top-level mounts for now */
		/* TODO: eliminate this limitation after ensuring that
		   UpdateQueue::Erase() really gets called for every
		   unmount, and no Directory disappears recursively
		   during database update */
		command_error(client, ACK_ERROR_ARG, "Bad mount point");
		return CommandResult::ERROR;
	}

	Error error;
	Storage *storage = CreateStorageURI(io_thread_get(), remote_uri,
					    error);
	if (storage == nullptr) {
		if (error.IsDefined())
			return print_error(client, error);

		command_error(client, ACK_ERROR_ARG,
			      "Unrecognized storage URI");
		return CommandResult::ERROR;
	}

	composite.Mount(local_uri, storage);
	idle_add(IDLE_MOUNT);

#ifdef ENABLE_DATABASE
	Database *_db = client.partition.instance.database;
	if (_db != nullptr && _db->IsPlugin(simple_db_plugin)) {
		SimpleDatabase &db = *(SimpleDatabase *)_db;

		if (!db.Mount(local_uri, remote_uri, error)) {
			composite.Unmount(local_uri);
			return print_error(client, error);
		}

		// TODO: call Instance::OnDatabaseModified()?
		// TODO: trigger database update?
		idle_add(IDLE_DATABASE);
	}
#endif

	return CommandResult::OK;
}

CommandResult
handle_unmount(Client &client, gcc_unused unsigned argc, char *argv[])
{
	Storage *_composite = client.partition.instance.storage;
	if (_composite == nullptr) {
		command_error(client, ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
	}

	CompositeStorage &composite = *(CompositeStorage *)_composite;

	const char *const local_uri = argv[1];

	if (*local_uri == 0) {
		command_error(client, ACK_ERROR_ARG, "Bad mount point");
		return CommandResult::ERROR;
	}

#ifdef ENABLE_DATABASE
	if (client.partition.instance.update != nullptr)
		/* ensure that no database update will attempt to work
		   with the database/storage instances we're about to
		   destroy here */
		client.partition.instance.update->CancelMount(local_uri);

	Database *_db = client.partition.instance.database;
	if (_db != nullptr && _db->IsPlugin(simple_db_plugin)) {
		SimpleDatabase &db = *(SimpleDatabase *)_db;

		if (db.Unmount(local_uri))
			// TODO: call Instance::OnDatabaseModified()?
			idle_add(IDLE_DATABASE);
	}
#endif

	if (!composite.Unmount(local_uri)) {
		command_error(client, ACK_ERROR_ARG, "Not a mount point");
		return CommandResult::ERROR;
	}

	idle_add(IDLE_MOUNT);

	return CommandResult::OK;
}
