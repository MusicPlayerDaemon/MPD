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
#include "SmbclientStorage.hxx"
#include "storage/StoragePlugin.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "lib/smbclient/Init.hxx"
#include "lib/smbclient/Mutex.hxx"
#include "fs/Traits.hxx"
#include "util/Error.hxx"
#include "thread/Mutex.hxx"

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
	const char *Read() override;
	bool GetInfo(bool follow, FileInfo &info, Error &error) override;
};

class SmbclientStorage final : public Storage {
	const std::string base;

	SMBCCTX *const ctx;

public:
	SmbclientStorage(const char *_base, SMBCCTX *_ctx)
		:base(_base), ctx(_ctx) {}

	virtual ~SmbclientStorage() {
		smbclient_mutex.lock();
		smbc_free_context(ctx, 1);
		smbclient_mutex.unlock();
	}

	/* virtual methods from class Storage */
	bool GetInfo(const char *uri_utf8, bool follow, FileInfo &info,
		     Error &error) override;

	StorageDirectoryReader *OpenDirectory(const char *uri_utf8,
					      Error &error) override;

	std::string MapUTF8(const char *uri_utf8) const override;

	const char *MapToRelativeUTF8(const char *uri_utf8) const override;
};

std::string
SmbclientStorage::MapUTF8(const char *uri_utf8) const
{
	assert(uri_utf8 != nullptr);

	if (*uri_utf8 == 0)
		return base;

	return PathTraitsUTF8::Build(base.c_str(), uri_utf8);
}

const char *
SmbclientStorage::MapToRelativeUTF8(const char *uri_utf8) const
{
	return PathTraitsUTF8::Relative(base.c_str(), uri_utf8);
}

static bool
GetInfo(const char *path, FileInfo &info, Error &error)
{
	struct stat st;
	smbclient_mutex.lock();
	bool success = smbc_stat(path, &st) == 0;
	smbclient_mutex.unlock();
	if (!success) {
		error.SetErrno();
		return false;
	}

	if (S_ISREG(st.st_mode))
		info.type = FileInfo::Type::REGULAR;
	else if (S_ISDIR(st.st_mode))
		info.type = FileInfo::Type::DIRECTORY;
	else
		info.type = FileInfo::Type::OTHER;

	info.size = st.st_size;
	info.mtime = st.st_mtime;
	info.device = st.st_dev;
	info.inode = st.st_ino;
	return true;
}

bool
SmbclientStorage::GetInfo(const char *uri_utf8, gcc_unused bool follow,
			  FileInfo &info, Error &error)
{
	const std::string mapped = MapUTF8(uri_utf8);
	return ::GetInfo(mapped.c_str(), info, error);
}

StorageDirectoryReader *
SmbclientStorage::OpenDirectory(const char *uri_utf8, Error &error)
{
	std::string mapped = MapUTF8(uri_utf8);
	smbclient_mutex.lock();
	int handle = smbc_opendir(mapped.c_str());
	smbclient_mutex.unlock();
	if (handle < 0) {
		error.SetErrno();
		return nullptr;
	}

	return new SmbclientDirectoryReader(std::move(mapped.c_str()), handle);
}

gcc_pure
static bool
SkipNameFS(const char *name)
{
	return name[0] == '.' &&
		(name[1] == 0 ||
		 (name[1] == '.' && name[2] == 0));
}

SmbclientDirectoryReader::~SmbclientDirectoryReader()
{
	smbclient_mutex.lock();
	smbc_close(handle);
	smbclient_mutex.unlock();
}

const char *
SmbclientDirectoryReader::Read()
{
	const ScopeLock protect(smbclient_mutex);

	struct smbc_dirent *e;
	while ((e = smbc_readdir(handle)) != nullptr) {
		name = e->name;
		if (!SkipNameFS(name))
			return name;
	}

	return nullptr;
}

bool
SmbclientDirectoryReader::GetInfo(gcc_unused bool follow, FileInfo &info,
				  Error &error)
{
	const std::string path = PathTraitsUTF8::Build(base.c_str(), name);
	return ::GetInfo(path.c_str(), info, error);
}

static Storage *
CreateSmbclientStorageURI(gcc_unused EventLoop &event_loop, const char *base,
			  Error &error)
{
	if (memcmp(base, "smb://", 6) != 0)
		return nullptr;

	if (!SmbclientInit(error))
		return nullptr;

	const ScopeLock protect(smbclient_mutex);
	SMBCCTX *ctx = smbc_new_context();
	if (ctx == nullptr) {
		error.SetErrno("smbc_new_context() failed");
		return nullptr;
	}

	SMBCCTX *ctx2 = smbc_init_context(ctx);
	if (ctx2 == nullptr) {
		error.SetErrno("smbc_init_context() failed");
		smbc_free_context(ctx, 1);
		return nullptr;
	}

	return new SmbclientStorage(base, ctx2);
}

const StoragePlugin smbclient_storage_plugin = {
	"smbclient",
	CreateSmbclientStorageURI,
};
