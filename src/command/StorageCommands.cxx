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
handle_listmounts(Client &client, gcc_unused int argc, gcc_unused char *argv[])
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
handle_mount(Client &client, gcc_unused int argc, char *argv[])
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

	Error error;
	Storage *storage = CreateStorageURI(remote_uri, error);
	if (storage == nullptr) {
		if (error.IsDefined())
			return print_error(client, error);

		command_error(client, ACK_ERROR_ARG,
			      "Unrecognized storage URI");
		return CommandResult::ERROR;
	}

	composite.Mount(local_uri, storage);
	return CommandResult::OK;
}
