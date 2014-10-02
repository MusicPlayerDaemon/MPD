// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LocalStorage.hxx"
#include "storage/StoragePlugin.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "fs/FileInfo.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/DirectoryReader.hxx"
#include "util/StringCompare.hxx"

#include <string>

class LocalDirectoryReader final : public StorageDirectoryReader {
	AllocatedPath base_fs;

	DirectoryReader reader;

	std::string name_utf8;

public:
	explicit LocalDirectoryReader(AllocatedPath &&_base_fs)
		:base_fs(std::move(_base_fs)), reader(base_fs) {}

	/* virtual methods from class StorageDirectoryReader */
	const char *Read() noexcept override;
	StorageFileInfo GetInfo(bool follow) override;
};

class LocalStorage final : public Storage {
	const AllocatedPath base_fs;
	const std::string base_utf8;

public:
	explicit LocalStorage(Path _base_fs)
		:base_fs(_base_fs), base_utf8(base_fs.ToUTF8Throw()) {
		assert(!base_fs.IsNull());
		assert(!base_utf8.empty());
	}

	/* virtual methods from class Storage */
	StorageFileInfo GetInfo(std::string_view uri_utf8, bool follow) override;

	std::unique_ptr<StorageDirectoryReader> OpenDirectory(std::string_view uri_utf8) override;

	[[nodiscard]] std::string MapUTF8(std::string_view uri_utf8) const noexcept override;

	[[nodiscard]] AllocatedPath MapFS(std::string_view uri_utf8) const noexcept override;

	[[nodiscard]] std::string_view MapToRelativeUTF8(std::string_view uri_utf8) const noexcept override;

	InputStreamPtr OpenFile(std::string_view uri_utf8, Mutex &mutex) override;

private:
	[[nodiscard]] AllocatedPath MapFSOrThrow(std::string_view uri_utf8) const;
};

static StorageFileInfo
Stat(Path path, bool follow)
{
	const FileInfo src(path, follow);
	StorageFileInfo info;

	if (src.IsRegular())
		info.type = StorageFileInfo::Type::REGULAR;
	else if (src.IsDirectory())
		info.type = StorageFileInfo::Type::DIRECTORY;
	else
		info.type = StorageFileInfo::Type::OTHER;

	info.size = src.GetSize();
	info.mtime = src.GetModificationTime();
#ifdef _WIN32
	info.device = info.inode = 0;
#else
	info.device = src.GetDevice();
	info.inode = src.GetInode();
#endif
	return info;
}

std::string
LocalStorage::MapUTF8(std::string_view uri_utf8) const noexcept
{
	if (uri_utf8.empty())
		return base_utf8;

	return PathTraitsUTF8::Build(base_utf8, uri_utf8);
}

AllocatedPath
LocalStorage::MapFSOrThrow(std::string_view uri_utf8) const
{
	if (uri_utf8.empty())
		return base_fs;

	return base_fs / AllocatedPath::FromUTF8Throw(uri_utf8);
}

AllocatedPath
LocalStorage::MapFS(std::string_view uri_utf8) const noexcept
{
	try {
		return MapFSOrThrow(uri_utf8);
	} catch (...) {
		return nullptr;
	}
}

std::string_view
LocalStorage::MapToRelativeUTF8(std::string_view uri_utf8) const noexcept
{
	return PathTraitsUTF8::Relative(base_utf8, uri_utf8);
}

InputStreamPtr
LocalStorage::OpenFile(std::string_view uri_utf8, Mutex &mutex)
{
	auto path = MapFS(uri_utf8);
	if (path == nullptr)
		return nullptr;

	return OpenLocalInputStream(path, mutex);
}

StorageFileInfo
LocalStorage::GetInfo(std::string_view uri_utf8, bool follow)
{
	return Stat(MapFSOrThrow(uri_utf8), follow);
}

std::unique_ptr<StorageDirectoryReader>
LocalStorage::OpenDirectory(std::string_view uri_utf8)
{
	return std::make_unique<LocalDirectoryReader>(MapFSOrThrow(uri_utf8));
}

const char *
LocalDirectoryReader::Read() noexcept
{
	while (reader.ReadEntry()) {
		const Path name_fs = reader.GetEntry();
		if (PathTraitsFS::IsSpecialFilename(name_fs.c_str()))
			continue;

		try {
			name_utf8 = name_fs.ToUTF8Throw();
			return name_utf8.c_str();
		} catch (...) {
		}
	}

	return nullptr;
}

StorageFileInfo
LocalDirectoryReader::GetInfo(bool follow)
{
	return Stat(base_fs / reader.GetEntry(), follow);
}

std::unique_ptr<Storage>
CreateLocalStorage(Path base_fs)
{
	return std::make_unique<LocalStorage>(base_fs);
}

constexpr StoragePlugin local_storage_plugin = {
	"local",
	nullptr,
	nullptr,
};
