/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "StorageInterface.hxx"
#include "thread/Mutex.hxx"
#include "util/Compiler.h"

#include <memory>
#include <string>
#include <map>

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
		 * The #Storage mounted in this virtual directory.  All
		 * "leaf" Directory instances must have a #Storage.
		 * Other Directory instances may have one, and child
		 * mounts will be "mixed" in.
		 */
		std::unique_ptr<Storage> storage;

		std::map<std::string, Directory> children;

		gcc_pure
		bool IsEmpty() const noexcept {
			return storage == nullptr && children.empty();
		}

		gcc_pure
		const Directory *Find(const char *uri) const noexcept;

		Directory &Make(const char *uri);

		bool Unmount() noexcept;
		bool Unmount(const char *uri) noexcept;

		gcc_pure
		bool MapToRelativeUTF8(std::string &buffer,
				       const char *uri) const noexcept;
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
	CompositeStorage() noexcept;
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
	Storage *GetMount(const char *uri) noexcept;

	/**
	 * Call the given function for each mounted storage, including
	 * the root storage.  Passes mount point URI and the a const
	 * Storage reference to the function.
	 */
	template<typename T>
	void VisitMounts(T t) const {
		const std::lock_guard<Mutex> protect(mutex);
		std::string uri;
		VisitMounts(uri, root, t);
	}

	void Mount(const char *uri, std::unique_ptr<Storage> storage);
	bool Unmount(const char *uri);

	/* virtual methods from class Storage */
	StorageFileInfo GetInfo(const char *uri, bool follow) override;

	std::unique_ptr<StorageDirectoryReader> OpenDirectory(const char *uri) override;

	std::string MapUTF8(const char *uri) const noexcept override;

	AllocatedPath MapFS(const char *uri) const noexcept override;

	const char *MapToRelativeUTF8(const char *uri) const noexcept override;

private:
	template<typename T>
	void VisitMounts(std::string &uri, const Directory &directory,
			 T t) const {
		if (directory.storage)
			t(uri.c_str(), *directory.storage);

		if (!uri.empty())
			uri.push_back('/');

		const size_t uri_length = uri.length();

		for (const auto &i : directory.children) {
			uri.resize(uri_length);
			uri.append(i.first);

			VisitMounts(uri, i.second, t);
		}
	}

	/**
	 * Follow the given URI path, and find the outermost directory
	 * which is a #Storage mount point.  If there are no mounts,
	 * it returns the root directory (with a nullptr "storage"
	 * attribute, of course).  FindResult::uri contains the
	 * remaining unused part of the URI (may be empty if all of
	 * the URI was used).
	 */
	gcc_pure
	FindResult FindStorage(const char *uri) const noexcept;

	const char *MapToRelativeUTF8(const Directory &directory,
				      const char *uri) const;
};

#endif
