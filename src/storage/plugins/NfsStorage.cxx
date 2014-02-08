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
#include "NfsStorage.hxx"
#include "storage/StoragePlugin.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "lib/nfs/Domain.hxx"
#include "util/Error.hxx"
#include "thread/Mutex.hxx"

extern "C" {
#include <nfsc/libnfs.h>
#include <nfsc/libnfs-raw-nfs.h>
}

#include <sys/stat.h>
#include <fcntl.h>

class NfsDirectoryReader final : public StorageDirectoryReader {
	const std::string base;

	nfs_context *ctx;
	nfsdir *dir;

	nfsdirent *ent;

public:
	NfsDirectoryReader(const char *_base, nfs_context *_ctx, nfsdir *_dir)
		:base(_base), ctx(_ctx), dir(_dir) {}

	virtual ~NfsDirectoryReader();

	/* virtual methods from class StorageDirectoryReader */
	virtual const char *Read() override;
	virtual bool GetInfo(bool follow, FileInfo &info,
			     Error &error) override;
};

class NfsStorage final : public Storage {
	const std::string base;

	nfs_context *ctx;

public:
	NfsStorage(const char *_base, nfs_context *_ctx)
		:base(_base), ctx(_ctx) {}

	virtual ~NfsStorage() {
		nfs_destroy_context(ctx);
	}

	/* virtual methods from class Storage */
	virtual bool GetInfo(const char *uri_utf8, bool follow, FileInfo &info,
			     Error &error) override;

	virtual StorageDirectoryReader *OpenDirectory(const char *uri_utf8,
						      Error &error) override;

	virtual std::string MapUTF8(const char *uri_utf8) const override;

	virtual const char *MapToRelativeUTF8(const char *uri_utf8) const override;
};

std::string
NfsStorage::MapUTF8(const char *uri_utf8) const
{
	assert(uri_utf8 != nullptr);

	if (*uri_utf8 == 0)
		return base;

	return PathTraitsUTF8::Build(base.c_str(), uri_utf8);
}

const char *
NfsStorage::MapToRelativeUTF8(const char *uri_utf8) const
{
	return PathTraitsUTF8::Relative(base.c_str(), uri_utf8);
}

static bool
GetInfo(nfs_context *ctx, const char *path, FileInfo &info, Error &error)
{
	struct stat st;
	int result = nfs_stat(ctx, path, &st);
	if (result < 0) {
		error.SetErrno(-result, "nfs_stat() failed");
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
NfsStorage::GetInfo(const char *uri_utf8, gcc_unused bool follow,
		    FileInfo &info, Error &error)
{
	/* libnfs paths must begin with a slash */
	std::string path(uri_utf8);
	path.insert(path.begin(), '/');

	return ::GetInfo(ctx, path.c_str(), info, error);
}

StorageDirectoryReader *
NfsStorage::OpenDirectory(const char *uri_utf8, Error &error)
{
	/* libnfs paths must begin with a slash */
	std::string path(uri_utf8);
	path.insert(path.begin(), '/');

	nfsdir *dir;
	int result = nfs_opendir(ctx, path.c_str(), &dir);
	if (result < 0) {
		error.SetErrno(-result, "nfs_opendir() failed");
		return nullptr;
	}

	return new NfsDirectoryReader(uri_utf8, ctx, dir);
}

gcc_pure
static bool
SkipNameFS(const char *name)
{
	return name[0] == '.' &&
		(name[1] == 0 ||
		 (name[1] == '.' && name[2] == 0));
}

NfsDirectoryReader::~NfsDirectoryReader()
{
	nfs_closedir(ctx, dir);
}

const char *
NfsDirectoryReader::Read()
{
	while ((ent = nfs_readdir(ctx, dir)) != nullptr) {
		if (!SkipNameFS(ent->name))
			return ent->name;
	}

	return nullptr;
}

bool
NfsDirectoryReader::GetInfo(gcc_unused bool follow, FileInfo &info,
			    gcc_unused Error &error)
{
	assert(ent != nullptr);

	switch (ent->type) {
	case NF3REG:
		info.type = FileInfo::Type::REGULAR;
		break;

	case NF3DIR:
		info.type = FileInfo::Type::DIRECTORY;
		break;

	default:
		info.type = FileInfo::Type::OTHER;
		break;
	}

	info.size = ent->size;
	info.mtime = ent->mtime.tv_sec;
	info.device = 0;
	info.inode = ent->inode;
	return true;
}

static Storage *
CreateNfsStorageURI(const char *base, Error &error)
{
	if (memcmp(base, "nfs://", 6) != 0)
		return nullptr;

	const char *p = base + 6;

	const char *mount = strchr(p, '/');
	if (mount == nullptr) {
		error.Set(nfs_domain, "Malformed nfs:// URI");
		return nullptr;
	}

	const std::string server(p, mount);

	nfs_context *ctx = nfs_init_context();
	if (ctx == nullptr) {
		error.Set(nfs_domain, "nfs_init_context() failed");
		return nullptr;
	}

	int result = nfs_mount(ctx, server.c_str(), mount);
	if (result < 0) {
		nfs_destroy_context(ctx);
		error.SetErrno(-result, "nfs_mount() failed");
		return nullptr;
	}

	return new NfsStorage(base, ctx);
}

const StoragePlugin nfs_storage_plugin = {
	"nfs",
	CreateNfsStorageURI,
};
