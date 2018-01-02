/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

/*
 * Save and load mounts of the compound storage to/from the state file.
 *
 */

#include "config.h"
#include "StorageState.hxx"
#include "fs/io/TextFile.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "storage/Registry.hxx"
#include "storage/CompositeStorage.hxx"
#include "db/plugins/simple/SimpleDatabasePlugin.hxx"
#include "util/StringCompare.hxx"
#include "util/Domain.hxx"
#include "Instance.hxx"
#include "IOThread.hxx"
#include "Log.hxx"

#include <set>
#include <boost/crc.hpp>

#define MOUNT_STATE_BEGIN        "mount_begin"
#define MOUNT_STATE_END          "mount_end"
#define MOUNT_STATE_STORAGE_URI  "uri: "
#define MOUNT_STATE_MOUNTED_URL  "mounted_url: "

static constexpr Domain storage_domain("storage");

void
storage_state_save(BufferedOutputStream &os, const Instance &instance)
{
	const auto visitor = [&os](const char *mount_uri, const Storage &storage) {
		std::string uri = storage.MapUTF8("");
		if (uri.empty() || StringIsEmpty(mount_uri))
			return;

		os.Format(
			MOUNT_STATE_BEGIN "\n"
			MOUNT_STATE_STORAGE_URI "%s\n"
			MOUNT_STATE_MOUNTED_URL "%s\n"
			MOUNT_STATE_END "\n", mount_uri, uri.c_str());
	};

	((CompositeStorage*)instance.storage)->VisitMounts(visitor);
}

bool
storage_state_restore(const char *line, TextFile &file, Instance &instance)
{
	if (!StringStartsWith(line, MOUNT_STATE_BEGIN))
		return false;

	std::string url;
	std::string uri;
	const char* value;

	while ((line = file.ReadLine()) != nullptr) {
		if (StringStartsWith(line, MOUNT_STATE_END))
			break;

		if ((value = StringAfterPrefix(line, MOUNT_STATE_MOUNTED_URL)))
			url = value;
		else if ((value = StringAfterPrefix(line, MOUNT_STATE_STORAGE_URI)))
			uri = value;
		else
			FormatError(storage_domain, "Unrecognized line in mountpoint state: %s", line);
	}

	if (url.empty() || uri.empty()) {
		LogError(storage_domain, "Missing value in mountpoint state.");	
		return true;
	}

	FormatDebug(storage_domain, "Restoring mount %s => %s", uri.c_str(), url.c_str());

	auto &event_loop = io_thread_get();
	Storage *storage = CreateStorageURI(event_loop, url.c_str());
	if (storage == nullptr) {
		FormatError(storage_domain, "Unrecognized storage URI: %s", url.c_str());
		return true;
	}

	Database *db = instance.database;
	if (db != nullptr && db->IsPlugin(simple_db_plugin)) {
		try {
			((SimpleDatabase *)db)->Mount(uri.c_str(), url.c_str());
		} catch (...) {
			throw;
		}
	}

	((CompositeStorage*)instance.storage)->Mount(uri.c_str(), storage);

	return true;
}

unsigned
storage_state_get_hash(const Instance &instance)
{
	std::set<std::string> mounts;

	const auto visitor = [&mounts](const char *mount_uri, const Storage &storage) {
		mounts.emplace(std::string(mount_uri) + ":" + storage.MapUTF8(""));
	};

	((CompositeStorage*)instance.storage)->VisitMounts(visitor);

	boost::crc_32_type result;

	for (auto mount: mounts) {
		result.process_bytes(mount.c_str(), mount.length());
	}

	return result.checksum();
}
