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

#include "SmbclientStorage.hxx"
#include "storage/StoragePlugin.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "lib/smbclient/Init.hxx"
#include "lib/smbclient/Mutex.hxx"
#include "fs/Traits.hxx"
#include "thread/Mutex.hxx"
#include "system/Error.hxx"
#include "util/ASCII.hxx"
#include "util/StringCompare.hxx"
#include "util/ScopeExit.hxx"

#include <libsmbclient.h>

class SmbclientDirectoryReader final : public StorageDirectoryReader {
	const std::string base;
	const unsigned handle;

	const char *name;

public:
	SmbclientDirectoryReader(std::string &&_base, unsigned _handle)
		:base(std::move(_base)), handle(_handle) {}

	virtual ~SmbclientDirectoryReader();

	/* virtual methods from class StorageDirectoryReader */
	const char *Read() noexcept override;
	StorageFileInfo GetInfo(bool follow) override;
};

class SmbclientStorage final : public Storage {
	const std::string base;

	SMBCCTX *const ctx;

public:
	SmbclientStorage(const char *_base, SMBCCTX *_ctx)
		:base(_base), ctx(_ctx) {}

	virtual ~SmbclientStorage() {
		const std::lock_guard<Mutex> lock(smbclient_mutex);
		smbc_free_context(ctx, 1);
	}

	/* virtual methods from class Storage */
	StorageFileInfo GetInfo(const char *uri_utf8, bool follow) override;

	std::unique_ptr<StorageDirectoryReader> OpenDirectory(const char *uri_utf8) override;

	std::string MapUTF8(const char *uri_utf8) const noexcept override;

	const char *MapToRelativeUTF8(const char *uri_utf8) const noexcept override;
};

std::string
SmbclientStorage::MapUTF8(const char *uri_utf8) const noexcept
{
	assert(uri_utf8 != nullptr);

	if (StringIsEmpty(uri_utf8))
		return base;

	return PathTraitsUTF8::Build(base.c_str(), uri_utf8);
}

const char *
SmbclientStorage::MapToRelativeUTF8(const char *uri_utf8) const noexcept
{
	return PathTraitsUTF8::Relative(base.c_str(), uri_utf8);
}

static StorageFileInfo
GetInfo(const char *path)
{
	struct stat st;

	{
		const std::lock_guard<Mutex> protect(smbclient_mutex);
		if (smbc_stat(path, &st) != 0)
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
SmbclientStorage::GetInfo(const char *uri_utf8, gcc_unused bool follow)
{
	const std::string mapped = MapUTF8(uri_utf8);
	return ::GetInfo(mapped.c_str());
}

std::unique_ptr<StorageDirectoryReader>
SmbclientStorage::OpenDirectory(const char *uri_utf8)
{
	std::string mapped = MapUTF8(uri_utf8);

	int handle;

	{
		const std::lock_guard<Mutex> protect(smbclient_mutex);
		handle = smbc_opendir(mapped.c_str());
		if (handle < 0)
			throw MakeErrno("Failed to open directory");
	}

	return std::make_unique<SmbclientDirectoryReader>(std::move(mapped.c_str()),
							  handle);
}

gcc_pure
static bool
SkipNameFS(const char *name) noexcept
{
	return name[0] == '.' &&
		(name[1] == 0 ||
		 (name[1] == '.' && name[2] == 0));
}

SmbclientDirectoryReader::~SmbclientDirectoryReader()
{
	const std::lock_guard<Mutex> lock(smbclient_mutex);
	smbc_close(handle);
}

const char *
SmbclientDirectoryReader::Read() noexcept
{
	const std::lock_guard<Mutex> protect(smbclient_mutex);

	struct smbc_dirent *e;
	while ((e = smbc_readdir(handle)) != nullptr) {
		name = e->name;
		if (!SkipNameFS(name))
			return name;
	}

	return nullptr;
}

StorageFileInfo
SmbclientDirectoryReader::GetInfo(gcc_unused bool follow)
{
	const std::string path = PathTraitsUTF8::Build(base.c_str(), name);
	return ::GetInfo(path.c_str());
}

static std::unique_ptr<Storage>
CreateSmbclientStorageURI(gcc_unused EventLoop &event_loop, const char *base)
{
	if (!StringStartsWithCaseASCII(base, "smb://"))
		return nullptr;

	SmbclientInit();

	const std::lock_guard<Mutex> protect(smbclient_mutex);
	SMBCCTX *ctx = smbc_new_context();
	if (ctx == nullptr)
		throw MakeErrno("smbc_new_context() failed");

	SMBCCTX *ctx2 = smbc_init_context(ctx);
	if (ctx2 == nullptr) {
		AtScopeExit(ctx) { smbc_free_context(ctx, 1); };
		throw MakeErrno("smbc_new_context() failed");
	}

	return std::make_unique<SmbclientStorage>(base, ctx2);
}

const StoragePlugin smbclient_storage_plugin = {
	"smbclient",
	CreateSmbclientStorageURI,
};
