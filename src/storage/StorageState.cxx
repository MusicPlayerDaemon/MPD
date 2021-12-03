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

/*
 * Save and load mounts of the compound storage to/from the state file.
 *
 */

#include "StorageState.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "io/LineReader.hxx"
#include "io/BufferedOutputStream.hxx"
#include "storage/Registry.hxx"
#include "storage/CompositeStorage.hxx"
#include "db/plugins/simple/SimpleDatabasePlugin.hxx"
#include "util/StringCompare.hxx"
#include "util/Domain.hxx"
#include "Instance.hxx"
#include "Log.hxx"

#ifdef __clang__
/* ignore -Wcomma due to strange code in boost/array.hpp (in Boost
   1.72) */
#pragma GCC diagnostic ignored "-Wcomma"
#endif

#include <boost/crc.hpp>

#include <set>

#define MOUNT_STATE_BEGIN        "mount_begin"
#define MOUNT_STATE_END          "mount_end"
#define MOUNT_STATE_STORAGE_URI  "uri: "
#define MOUNT_STATE_MOUNTED_URL  "mounted_url: "

static constexpr Domain storage_domain("storage");

void
storage_state_save(BufferedOutputStream &os, const Instance &instance)
{
	if (instance.storage == nullptr)
		return;

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
storage_state_restore(const char *line, LineReader &file, Instance &instance)
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
			FmtError(storage_domain,
				 "Unrecognized line in mountpoint state: {}",
				 line);
	}

	if (instance.storage == nullptr)
		/* without storage (a CompositeStorage instance), we
		   cannot mount, and therefore we silently ignore the
		   state file */
		return true;

	if (url.empty() || uri.empty()) {
		LogError(storage_domain, "Missing value in mountpoint state.");	
		return true;
	}

	FmtDebug(storage_domain, "Restoring mount {} => {}", uri, url);

	auto &composite_storage = *(CompositeStorage *)instance.storage;
	if (composite_storage.IsMountPoint(uri.c_str())) {
		LogError(storage_domain, "Mount point busy");
		return true;
	}

	if (composite_storage.IsMounted(url.c_str())) {
		LogError(storage_domain, "This storage is already mounted");
		return true;
	}

	auto &event_loop = instance.io_thread.GetEventLoop();
	auto storage = CreateStorageURI(event_loop, url.c_str());
	if (storage == nullptr) {
		FmtError(storage_domain, "Unrecognized storage URI: {}", url);
		return true;
	}

	if (auto *db = dynamic_cast<SimpleDatabase *>(instance.GetDatabase())) {
		try {
			db->Mount(uri.c_str(), url.c_str());
		} catch (...) {
			FmtError(storage_domain,
				 "Failed to restore mount to {}: {}",
				 url, std::current_exception());
			return true;
		}
	}

	composite_storage.Mount(uri.c_str(), std::move(storage));

	return true;
}

unsigned
storage_state_get_hash(const Instance &instance)
{
	if (instance.storage == nullptr)
		return 0;

	std::set<std::string> mounts;

	const auto visitor = [&mounts](const char *mount_uri, const Storage &storage) {
		mounts.emplace(std::string(mount_uri) + ":" + storage.MapUTF8(""));
	};

	((CompositeStorage*)instance.storage)->VisitMounts(visitor);

	boost::crc_32_type result;

	for (const auto& mount : mounts) {
		result.process_bytes(mount.c_str(), mount.length());
	}

	return result.checksum();
}
