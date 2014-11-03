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
#include "CompositeStorage.hxx"
#include "FileInfo.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <set>

#include <string.h>

static constexpr Domain composite_domain("composite");

/**
 * Combines the directory entries of another #StorageDirectoryReader
 * instance and the virtual directory entries.
 */
class CompositeDirectoryReader final : public StorageDirectoryReader {
	StorageDirectoryReader *other;

	std::set<std::string> names;
	std::set<std::string>::const_iterator current, next;

public:
	template<typename M>
	CompositeDirectoryReader(StorageDirectoryReader *_other,
				 const M &map)
		:other(_other) {
		for (const auto &i : map)
			names.insert(i.first);
		next = names.begin();
	}

	virtual ~CompositeDirectoryReader() {
		delete other;
	}

	/* virtual methods from class StorageDirectoryReader */
	const char *Read() override;
	bool GetInfo(bool follow, FileInfo &info, Error &error) override;
};

const char *
CompositeDirectoryReader::Read()
{
	if (other != nullptr) {
		const char *name = other->Read();
		if (name != nullptr) {
			names.erase(name);
			return name;
		}

		delete other;
		other = nullptr;
	}

	if (next == names.end())
		return nullptr;

	current = next++;
	return current->c_str();
}

bool
CompositeDirectoryReader::GetInfo(bool follow, FileInfo &info,
				  Error &error)
{
	if (other != nullptr)
		return other->GetInfo(follow, info, error);

	assert(current != names.end());

	info.type = FileInfo::Type::DIRECTORY;
	info.mtime = 0;
	info.device = 0;
	info.inode = 0;
	return true;
}

static std::string
NextSegment(const char *&uri_r)
{
	const char *uri = uri_r;
	const char *slash = strchr(uri, '/');
	if (slash == nullptr) {
		uri_r += strlen(uri);
		return std::string(uri);
	} else {
		uri_r = slash + 1;
		return std::string(uri, slash);
	}
}

CompositeStorage::Directory::~Directory()
{
	delete storage;
}

const CompositeStorage::Directory *
CompositeStorage::Directory::Find(const char *uri) const
{
	const Directory *directory = this;
	while (*uri != 0) {
		const std::string name = NextSegment(uri);
		auto i = directory->children.find(name);
		if (i == directory->children.end())
			return nullptr;

		directory = &i->second;
	}

	return directory;
}

CompositeStorage::Directory &
CompositeStorage::Directory::Make(const char *uri)
{
	Directory *directory = this;
	while (*uri != 0) {
		const std::string name = NextSegment(uri);
#if defined(__clang__) || GCC_CHECK_VERSION(4,8)
		auto i = directory->children.emplace(std::move(name),
						     Directory());
#else
		auto i = directory->children.insert(std::make_pair(std::move(name),
								   Directory()));
#endif
		directory = &i.first->second;
	}

	return *directory;
}

bool
CompositeStorage::Directory::Unmount()
{
	if (storage == nullptr)
		return false;

	delete storage;
	storage = nullptr;
	return true;
}

bool
CompositeStorage::Directory::Unmount(const char *uri)
{
	if (*uri == 0)
		return Unmount();

	const std::string name = NextSegment(uri);

	auto i = children.find(name);
	if (i == children.end() || !i->second.Unmount(uri))
		return false;

	if (i->second.IsEmpty())
		children.erase(i);

	return true;

}

bool
CompositeStorage::Directory::MapToRelativeUTF8(std::string &buffer,
					       const char *uri) const
{
	if (storage != nullptr) {
		const char *result = storage->MapToRelativeUTF8(uri);
		if (result != nullptr) {
			buffer = result;
			return true;
		}
	}

	for (const auto &i : children) {
		if (i.second.MapToRelativeUTF8(buffer, uri)) {
			buffer.insert(buffer.begin(), '/');
			buffer.insert(buffer.begin(),
				      i.first.begin(), i.first.end());
			return true;
		}
	}

	return false;
}

CompositeStorage::CompositeStorage()
{
}

CompositeStorage::~CompositeStorage()
{
}

Storage *
CompositeStorage::GetMount(const char *uri)
{
	const ScopeLock protect(mutex);

	auto result = FindStorage(uri);
	if (*result.uri != 0)
		/* not a mount point */
		return nullptr;

	return result.directory->storage;
}

void
CompositeStorage::Mount(const char *uri, Storage *storage)
{
	const ScopeLock protect(mutex);

	Directory &directory = root.Make(uri);
	if (directory.storage != nullptr)
		delete directory.storage;
	directory.storage = storage;
}

bool
CompositeStorage::Unmount(const char *uri)
{
	const ScopeLock protect(mutex);

	return root.Unmount(uri);
}

CompositeStorage::FindResult
CompositeStorage::FindStorage(const char *uri) const
{
	FindResult result{&root, uri};

	const Directory *directory = &root;
	while (*uri != 0) {
		const std::string name = NextSegment(uri);

		auto i = directory->children.find(name);
		if (i == directory->children.end())
			break;

		directory = &i->second;
		if (directory->storage != nullptr)
			result = FindResult{directory, uri};
	}

	return result;
}

CompositeStorage::FindResult
CompositeStorage::FindStorage(const char *uri, Error &error) const
{
	auto result = FindStorage(uri);
	if (result.directory == nullptr)
		error.Set(composite_domain, "No such directory");
	return result;
}

bool
CompositeStorage::GetInfo(const char *uri, bool follow, FileInfo &info,
			  Error &error)
{
	const ScopeLock protect(mutex);

	auto f = FindStorage(uri, error);
	if (f.directory->storage != nullptr &&
	    f.directory->storage->GetInfo(f.uri, follow, info, error))
		return true;

	const Directory *directory = f.directory->Find(f.uri);
	if (directory != nullptr) {
		error.Clear();
		info.type = FileInfo::Type::DIRECTORY;
		info.mtime = 0;
		info.device = 0;
		info.inode = 0;
		return true;
	}

	return false;
}

StorageDirectoryReader *
CompositeStorage::OpenDirectory(const char *uri,
				Error &error)
{
	const ScopeLock protect(mutex);

	auto f = FindStorage(uri, error);
	const Directory *directory = f.directory->Find(f.uri);
	if (directory == nullptr || directory->children.empty()) {
		/* no virtual directories here */

		if (f.directory->storage == nullptr)
			return nullptr;

		return f.directory->storage->OpenDirectory(f.uri, error);
	}

	StorageDirectoryReader *other =
		f.directory->storage->OpenDirectory(f.uri, IgnoreError());
	return new CompositeDirectoryReader(other, directory->children);
}

std::string
CompositeStorage::MapUTF8(const char *uri) const
{
	const ScopeLock protect(mutex);

	auto f = FindStorage(uri);
	if (f.directory->storage == nullptr)
		return std::string();

	return f.directory->storage->MapUTF8(f.uri);
}

AllocatedPath
CompositeStorage::MapFS(const char *uri) const
{
	const ScopeLock protect(mutex);

	auto f = FindStorage(uri);
	if (f.directory->storage == nullptr)
		return AllocatedPath::Null();

	return f.directory->storage->MapFS(f.uri);
}

const char *
CompositeStorage::MapToRelativeUTF8(const char *uri) const
{
	const ScopeLock protect(mutex);

	if (root.storage != nullptr) {
		const char *result = root.storage->MapToRelativeUTF8(uri);
		if (result != nullptr)
			return result;
	}

	if (!root.MapToRelativeUTF8(relative_buffer, uri))
		return nullptr;

	return relative_buffer.c_str();
}
