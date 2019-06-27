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

#include "CompositeStorage.hxx"
#include "FileInfo.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/StringCompare.hxx"

#include <set>
#include <stdexcept>

#include <string.h>

/**
 * Combines the directory entries of another #StorageDirectoryReader
 * instance and the virtual directory entries.
 */
class CompositeDirectoryReader final : public StorageDirectoryReader {
	std::unique_ptr<StorageDirectoryReader> other;

	std::set<std::string> names;
	std::set<std::string>::const_iterator current, next;

public:
	template<typename O, typename M>
	CompositeDirectoryReader(O &&_other, const M &map)
		:other(std::forward<O>(_other)) {
		for (const auto &i : map)
			names.insert(i.first);
		next = names.begin();
	}

	/* virtual methods from class StorageDirectoryReader */
	const char *Read() noexcept override;
	StorageFileInfo GetInfo(bool follow) override;
};

const char *
CompositeDirectoryReader::Read() noexcept
{
	if (other != nullptr) {
		const char *name = other->Read();
		if (name != nullptr) {
			names.erase(name);
			return name;
		}

		other.reset();
	}

	if (next == names.end())
		return nullptr;

	current = next++;
	return current->c_str();
}

StorageFileInfo
CompositeDirectoryReader::GetInfo(bool follow)
{
	if (other != nullptr)
		return other->GetInfo(follow);

	assert(current != names.end());

	return StorageFileInfo(StorageFileInfo::Type::DIRECTORY);
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

const CompositeStorage::Directory *
CompositeStorage::Directory::Find(const char *uri) const noexcept
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
		auto i = directory->children.emplace(std::move(name),
						     Directory());
		directory = &i.first->second;
	}

	return *directory;
}

bool
CompositeStorage::Directory::Unmount() noexcept
{
	if (storage == nullptr)
		return false;

	storage.reset();
	return true;
}

bool
CompositeStorage::Directory::Unmount(const char *uri) noexcept
{
	if (StringIsEmpty(uri))
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
					       const char *uri) const noexcept
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

CompositeStorage::CompositeStorage() noexcept
{
}

CompositeStorage::~CompositeStorage()
{
}

Storage *
CompositeStorage::GetMount(const char *uri) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	auto result = FindStorage(uri);
	if (*result.uri != 0)
		/* not a mount point */
		return nullptr;

	return result.directory->storage.get();
}

void
CompositeStorage::Mount(const char *uri, std::unique_ptr<Storage> storage)
{
	const std::lock_guard<Mutex> protect(mutex);

	Directory &directory = root.Make(uri);
	directory.storage = std::move(storage);
}

bool
CompositeStorage::Unmount(const char *uri)
{
	const std::lock_guard<Mutex> protect(mutex);

	return root.Unmount(uri);
}

CompositeStorage::FindResult
CompositeStorage::FindStorage(const char *uri) const noexcept
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

StorageFileInfo
CompositeStorage::GetInfo(const char *uri, bool follow)
{
	const std::lock_guard<Mutex> protect(mutex);

	std::exception_ptr error;

	auto f = FindStorage(uri);
	if (f.directory->storage != nullptr) {
		try {
			return f.directory->storage->GetInfo(f.uri, follow);
		} catch (...) {
			error = std::current_exception();
		}
	}

	const Directory *directory = f.directory->Find(f.uri);
	if (directory != nullptr)
		return StorageFileInfo(StorageFileInfo::Type::DIRECTORY);

	if (error)
		std::rethrow_exception(error);
	else
		throw std::runtime_error("No such file or directory");
}

std::unique_ptr<StorageDirectoryReader>
CompositeStorage::OpenDirectory(const char *uri)
{
	const std::lock_guard<Mutex> protect(mutex);

	auto f = FindStorage(uri);
	const Directory *directory = f.directory->Find(f.uri);
	if (directory == nullptr || directory->children.empty()) {
		/* no virtual directories here */

		if (f.directory->storage == nullptr)
			throw std::runtime_error("No such directory");

		return f.directory->storage->OpenDirectory(f.uri);
	}

	std::unique_ptr<StorageDirectoryReader> other;

	try {
		other = f.directory->storage->OpenDirectory(f.uri);
	} catch (...) {
	}

	return std::make_unique<CompositeDirectoryReader>(std::move(other),
							  directory->children);
}

std::string
CompositeStorage::MapUTF8(const char *uri) const noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	auto f = FindStorage(uri);
	if (f.directory->storage == nullptr)
		return std::string();

	return f.directory->storage->MapUTF8(f.uri);
}

AllocatedPath
CompositeStorage::MapFS(const char *uri) const noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	auto f = FindStorage(uri);
	if (f.directory->storage == nullptr)
		return nullptr;

	return f.directory->storage->MapFS(f.uri);
}

const char *
CompositeStorage::MapToRelativeUTF8(const char *uri) const noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	if (root.storage != nullptr) {
		const char *result = root.storage->MapToRelativeUTF8(uri);
		if (result != nullptr)
			return result;
	}

	if (!root.MapToRelativeUTF8(relative_buffer, uri))
		return nullptr;

	return relative_buffer.c_str();
}
