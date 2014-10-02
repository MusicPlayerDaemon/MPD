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

#ifndef MPD_COMPOSITE_STORAGE_HXX
#define MPD_COMPOSITE_STORAGE_HXX

#include "check.h"
#include "StorageInterface.hxx"
#include "thread/Mutex.hxx"
#include "Compiler.h"

#include <string>
#include <map>

class Error;
class Storage;

/**
 * A #Storage implementation that combines multiple other #Storage
 * instances in one virtual tree.  It is used to "mount" new #Storage
 * instances into the storage tree.
 *
 * This class is thread-safe: mounts may be added and removed at any
 * time in any thread.
 */
class CompositeStorage final : public Storage {
	/**
	 * A node in the virtual directory tree.
	 */
	struct Directory {
		/**
		 * The #Storage mounted n this virtual directory.  All
		 * "leaf" Directory instances must have a #Storage.
		 * Other Directory instances may have one, and child
		 * mounts will be "mixed" in.
		 */
		Storage *storage;

		std::map<std::string, Directory> children;

		Directory():storage(nullptr) {}
		~Directory();

		gcc_pure
		bool IsEmpty() const {
			return storage == nullptr && children.empty();
		}

		gcc_pure
		const Directory *Find(const char *uri) const;

		Directory &Make(const char *uri);

		bool Unmount();
		bool Unmount(const char *uri);

		gcc_pure
		bool MapToRelativeUTF8(std::string &buffer,
				       const char *uri) const;
	};

	struct FindResult {
		const Directory *directory;
		const char *uri;
	};

	/**
	 * Protects the virtual #Directory tree.
	 *
	 * TODO: use readers-writer lock
	 */
	mutable Mutex mutex;

	Directory root;

	mutable std::string relative_buffer;

public:
	CompositeStorage();
	virtual ~CompositeStorage();

	/**
	 * Get the #Storage at the specified mount point.  Returns
	 * nullptr if the given URI is not a mount point.
	 *
	 * The returned pointer is unprotected.  No other thread is
	 * allowed to unmount the given mount point while the return
	 * value is being used.
	 */
	gcc_pure gcc_nonnull_all
	Storage *GetMount(const char *uri);

	/**
	 * Call the given function for each mounted storage, including
	 * the root storage.  Passes mount point URI and the a const
	 * Storage reference to the function.
	 */
	template<typename T>
	void VisitMounts(T t) const {
		const ScopeLock protect(mutex);
		std::string uri;
		VisitMounts(uri, root, t);
	}

	void Mount(const char *uri, Storage *storage);
	bool Unmount(const char *uri);

	/* virtual methods from class Storage */
	bool GetInfo(const char *uri, bool follow, FileInfo &info,
		     Error &error) override;

	StorageDirectoryReader *OpenDirectory(const char *uri,
					      Error &error) override;

	std::string MapUTF8(const char *uri) const override;

	AllocatedPath MapFS(const char *uri) const override;

	const char *MapToRelativeUTF8(const char *uri) const override;

private:
	template<typename T>
	void VisitMounts(std::string &uri, const Directory &directory,
			 T t) const {
		const Storage *const storage = directory.storage;
		if (storage != nullptr)
			t(uri.c_str(), *storage);

		if (!uri.empty())
			uri.push_back('/');

		const size_t uri_length = uri.length();

		for (const auto &i : directory.children) {
			uri.resize(uri_length);
			uri.append(i.first);

			VisitMounts(uri, i.second, t);
		}
	}

	gcc_pure
	FindResult FindStorage(const char *uri) const;
	FindResult FindStorage(const char *uri, Error &error) const;

	const char *MapToRelativeUTF8(const Directory &directory,
				      const char *uri) const;
};

#endif
