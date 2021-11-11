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

#include "CompositeStorage.hxx"
#include "FileInfo.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/IterableSplitString.hxx"
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

static std::string_view
NextSegment(std::string_view &uri_r) noexcept
{
	StringView uri(uri_r);
	auto s = uri.Split('/');
	uri_r = s.second;
	return s.first;
}

const CompositeStorage::Directory *
CompositeStorage::Directory::Find(std::string_view uri) const noexcept
{
	const Directory *directory = this;

	for (std::string_view name : IterableSplitString(uri, '/')) {
		if (name.empty())
			continue;

		auto i = directory->children.find(name);
		if (i == directory->children.end())
			return nullptr;

		directory = &i->second;
	}

	return directory;
}

CompositeStorage::Directory &
CompositeStorage::Directory::Make(std::string_view uri)
{
	Directory *directory = this;

	for (std::string_view name : IterableSplitString(uri, '/')) {
		if (name.empty())
			continue;

		auto i = directory->children.emplace(name, Directory());
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
CompositeStorage::Directory::Unmount(std::string_view uri) noexcept
{
	if (uri.empty())
		return Unmount();

	const auto name = NextSegment(uri);

	auto i = children.find(name);
	if (i == children.end() || !i->second.Unmount(uri))
		return false;

	if (i->second.IsEmpty())
		children.erase(i);

	return true;

}

bool
CompositeStorage::Directory::MapToRelativeUTF8(std::string &buffer,
					       std::string_view uri) const noexcept
{
	if (storage != nullptr) {
		auto result = storage->MapToRelativeUTF8(uri);
		if (result.data() != nullptr) {
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
	/* note: no "=default" here because members of this class are
	   allowed to throw during construction according to the C++
	   standard (e.g. std::map), but we choose to ignore these
	   exceptions; if construction of std::map goes wrong, MPD has
	   no chance to work at all, so it's ok to std::terminate() */
}

CompositeStorage::~CompositeStorage() = default;

Storage *
CompositeStorage::GetMount(std::string_view uri) noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	auto result = FindStorage(uri);
	if (!result.uri.empty())
		/* not a mount point */
		return nullptr;

	return result.directory->storage.get();
}

void
CompositeStorage::Mount(const char *uri, std::unique_ptr<Storage> storage)
{
	const std::scoped_lock<Mutex> protect(mutex);

	Directory &directory = root.Make(uri);
	assert(!directory.storage);
	directory.storage = std::move(storage);
}

bool
CompositeStorage::Unmount(const char *uri)
{
	const std::scoped_lock<Mutex> protect(mutex);

	return root.Unmount(uri);
}

CompositeStorage::FindResult
CompositeStorage::FindStorage(std::string_view uri) const noexcept
{
	FindResult result{&root, uri};

	const Directory *directory = &root;
	while (!uri.empty()) {
		const auto name = NextSegment(uri);

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
CompositeStorage::GetInfo(std::string_view uri, bool follow)
{
	const std::scoped_lock<Mutex> protect(mutex);

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
CompositeStorage::OpenDirectory(std::string_view uri)
{
	const std::scoped_lock<Mutex> protect(mutex);

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
CompositeStorage::MapUTF8(std::string_view uri) const noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	auto f = FindStorage(uri);
	if (f.directory->storage == nullptr)
		return {};

	return f.directory->storage->MapUTF8(f.uri);
}

AllocatedPath
CompositeStorage::MapFS(std::string_view uri) const noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	auto f = FindStorage(uri);
	if (f.directory->storage == nullptr)
		return nullptr;

	return f.directory->storage->MapFS(f.uri);
}

std::string_view
CompositeStorage::MapToRelativeUTF8(std::string_view uri) const noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	if (root.storage != nullptr) {
		auto result = root.storage->MapToRelativeUTF8(uri);
		if (result.data() != nullptr)
			return result;
	}

	if (!root.MapToRelativeUTF8(relative_buffer, uri))
		return {};

	return relative_buffer;
}
