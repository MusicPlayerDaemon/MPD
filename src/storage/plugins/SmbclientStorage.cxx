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

#include "SmbclientStorage.hxx"
#include "storage/StoragePlugin.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "lib/smbclient/Init.hxx"
#include "lib/smbclient/Context.hxx"
#include "fs/Traits.hxx"
#include "thread/Mutex.hxx"
#include "system/Error.hxx"
#include "util/ASCII.hxx"
#include "util/StringCompare.hxx"
#include "util/ScopeExit.hxx"

#include <libsmbclient.h>

class SmbclientStorage;

class SmbclientDirectoryReader final : public StorageDirectoryReader {
	SmbclientStorage &storage;
	const std::string base;
	SMBCFILE *const handle;

	const char *name;

public:
	SmbclientDirectoryReader(SmbclientStorage &_storage,
				 std::string &&_base,
				 SMBCFILE *_handle) noexcept
		:storage(_storage), base(std::move(_base)), handle(_handle) {}

	~SmbclientDirectoryReader() override;

	/* virtual methods from class StorageDirectoryReader */
	const char *Read() noexcept override;
	StorageFileInfo GetInfo(bool follow) override;
};

class SmbclientStorage final : public Storage {
	friend class SmbclientDirectoryReader;

	const std::string base;

	/**
	 * This mutex protects all calls into the #SmbclientContext,
	 * which is not thread-safe.
	 */
	Mutex mutex;

	SmbclientContext ctx = SmbclientContext::New();

public:
	explicit SmbclientStorage(const char *_base)
		:base(_base) {}

	/* virtual methods from class Storage */
	StorageFileInfo GetInfo(std::string_view uri_utf8, bool follow) override;

	std::unique_ptr<StorageDirectoryReader> OpenDirectory(std::string_view uri_utf8) override;

	[[nodiscard]] std::string MapUTF8(std::string_view uri_utf8) const noexcept override;

	[[nodiscard]] std::string_view MapToRelativeUTF8(std::string_view uri_utf8) const noexcept override;
};

std::string
SmbclientStorage::MapUTF8(std::string_view uri_utf8) const noexcept
{
	if (uri_utf8.empty())
		return base;

	return PathTraitsUTF8::Build(base, uri_utf8);
}

std::string_view
SmbclientStorage::MapToRelativeUTF8(std::string_view uri_utf8) const noexcept
{
	return PathTraitsUTF8::Relative(base, uri_utf8);
}

static StorageFileInfo
GetInfo(SmbclientContext &ctx, Mutex &mutex, const char *path)
{
	struct stat st;

	{
		const std::scoped_lock<Mutex> protect(mutex);
		if (ctx.Stat(path, st) != 0)
			throw MakeErrno("Failed to access file");
	}

	StorageFileInfo info;
	if (S_ISREG(st.st_mode))
		info.type = StorageFileInfo::Type::REGULAR;
	else if (S_ISDIR(st.st_mode))
		info.type = StorageFileInfo::Type::DIRECTORY;
	else
		info.type = StorageFileInfo::Type::OTHER;

	info.size = st.st_size;
	info.mtime = std::chrono::system_clock::from_time_t(st.st_mtime);
	info.device = st.st_dev;
	info.inode = st.st_ino;
	return info;
}

StorageFileInfo
SmbclientStorage::GetInfo(std::string_view uri_utf8, [[maybe_unused]] bool follow)
{
	const std::string mapped = MapUTF8(uri_utf8);
	return ::GetInfo(ctx, mutex, mapped.c_str());
}

std::unique_ptr<StorageDirectoryReader>
SmbclientStorage::OpenDirectory(std::string_view uri_utf8)
{
	std::string mapped = MapUTF8(uri_utf8);

	SMBCFILE *handle;

	{
		const std::scoped_lock<Mutex> protect(mutex);
		handle = ctx.OpenDirectory(mapped.c_str());
	}

	if (handle == nullptr)
		throw MakeErrno("Failed to open directory");

	return std::make_unique<SmbclientDirectoryReader>(*this,
							  std::move(mapped),
							  handle);
}

gcc_pure
static bool
SkipNameFS(PathTraitsFS::const_pointer name) noexcept
{
	return PathTraitsFS::IsSpecialFilename(name);
}

SmbclientDirectoryReader::~SmbclientDirectoryReader()
{
	const std::scoped_lock<Mutex> lock(storage.mutex);
	storage.ctx.CloseDirectory(handle);
}

const char *
SmbclientDirectoryReader::Read() noexcept
{
	const std::scoped_lock<Mutex> protect(storage.mutex);

	while (auto e = storage.ctx.ReadDirectory(handle)) {
		name = e->name;
		if (!SkipNameFS(name))
			return name;
	}

	return nullptr;
}

StorageFileInfo
SmbclientDirectoryReader::GetInfo([[maybe_unused]] bool follow)
{
	const std::string path = PathTraitsUTF8::Build(base, name);
	return ::GetInfo(storage.ctx, storage.mutex, path.c_str());
}

static std::unique_ptr<Storage>
CreateSmbclientStorageURI([[maybe_unused]] EventLoop &event_loop, const char *base)
{
	SmbclientInit();

	return std::make_unique<SmbclientStorage>(base);
}

static constexpr const char *smbclient_prefixes[] = { "smb://", nullptr };

const StoragePlugin smbclient_storage_plugin = {
	"smbclient",
	smbclient_prefixes,
	CreateSmbclientStorageURI,
};
