// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

#include <functional> // for std::hash()

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

		os.Fmt(FMT_STRING(MOUNT_STATE_BEGIN "\n"
				  MOUNT_STATE_STORAGE_URI "{}\n"
				  MOUNT_STATE_MOUNTED_URL "{}\n"
				  MOUNT_STATE_END "\n"),
		       mount_uri, uri);
	};

	((CompositeStorage*)instance.storage)->VisitMounts(visitor);
}

bool
storage_state_restore(const char *line, LineReader &file,
		      Instance &instance) noexcept
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
storage_state_get_hash(const Instance &instance) noexcept
{
	if (instance.storage == nullptr)
		return 0;

	unsigned result = 0;

	const std::hash<std::string_view> hash;

	const auto visitor = [&result, &hash](const char *mount_uri, const Storage &storage) {
		result = result * 33 + hash(mount_uri);
		result = result * 33 + hash(storage.MapUTF8(""));
	};

	((CompositeStorage*)instance.storage)->VisitMounts(visitor);

	return result;
}
