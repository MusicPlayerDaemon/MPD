// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef UDISKS2_HXX
#define UDISKS2_HXX

#include <string>
#include <functional>

#define UDISKS2_PATH "/org/freedesktop/UDisks2"
#define UDISKS2_INTERFACE "org.freedesktop.UDisks2"
#define UDISKS2_FILESYSTEM_INTERFACE "org.freedesktop.UDisks2.Filesystem"

namespace ODBus {
class Message;
class ReadMessageIter;
}

namespace UDisks2 {

struct Object {
	std::string path;

	std::string drive_id, block_id;

	/**
	 * The first element of the "MountPoints" array of the
	 * "Filesystem" interface.  Empty if no "MountPoints" property
	 * exists.
	 */
	std::string mount_point;

	bool is_filesystem = false;

	explicit Object(const char *_path) noexcept
		:path(_path) {}

	bool IsValid() const noexcept {
		return is_filesystem &&
			(!drive_id.empty() || !block_id.empty());
	}

	template<typename I>
	bool IsId(I &&other) const noexcept {
		if (!drive_id.empty())
			return drive_id == std::forward<I>(other);
		else if (!block_id.empty())
			return block_id == std::forward<I>(other);
		else
			return false;
	}

	std::string GetUri() const noexcept {
		if (!drive_id.empty())
			return "udisks://" + drive_id;
		else if (!block_id.empty())
			return "udisks://" + block_id;
		else
			return {};
	}
};

void
ParseObject(Object &o, ODBus::ReadMessageIter &&i) noexcept;

/**
 * Parse objects from an array/dictionary and invoke the callback for
 * each.
 */
void
ParseObjects(ODBus::ReadMessageIter &&i,
	     std::function<void(Object &&o)> callback);

/**
 * Parse objects from a GetManagedObjects reply and invoke the
 * callback for each.
 *
 * Throws on error.
 */
void
ParseObjects(ODBus::Message &reply,
	     std::function<void(Object &&o)> callback);

} // namespace UDisks2

#endif
