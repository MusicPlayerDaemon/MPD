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
#include "LocalStorage.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "util/Error.hxx"
#include "fs/FileSystem.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/DirectoryReader.hxx"

#include <string>

class LocalDirectoryReader final : public StorageDirectoryReader {
	AllocatedPath base_fs;

	DirectoryReader reader;

	std::string name_utf8;

public:
	LocalDirectoryReader(AllocatedPath &&_base_fs)
		:base_fs(std::move(_base_fs)), reader(base_fs) {}

	bool HasFailed() {
		return reader.HasFailed();
	}

	/* virtual methods from class StorageDirectoryReader */
	virtual const char *Read() override;
	virtual bool GetInfo(bool follow, FileInfo &info,
			     Error &error) override;
};

class LocalStorage final : public Storage {
	const std::string base_utf8;
	const AllocatedPath base_fs;

public:
	LocalStorage(const char *_base_utf8, Path _base_fs)
		:base_utf8(_base_utf8), base_fs(_base_fs) {}

	/* virtual methods from class Storage */
	virtual bool GetInfo(const char *uri_utf8, bool follow, FileInfo &info,
			     Error &error) override;

	virtual StorageDirectoryReader *OpenDirectory(const char *uri_utf8,
						      Error &error) override;

	virtual std::string MapUTF8(const char *uri_utf8) const override;

	virtual AllocatedPath MapFS(const char *uri_utf8) const override;

	virtual const char *MapToRelativeUTF8(const char *uri_utf8) const override;

private:
	AllocatedPath MapFS(const char *uri_utf8, Error &error) const;
};

static bool
Stat(Path path, bool follow, FileInfo &info, Error &error)
{
	struct stat st;
	if (!StatFile(path, st, follow)) {
		error.SetErrno();

		const auto path_utf8 = path.ToUTF8();
		error.FormatPrefix("Failed to stat %s: ", path_utf8.c_str());
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

std::string
LocalStorage::MapUTF8(const char *uri_utf8) const
{
	assert(uri_utf8 != nullptr);

	if (*uri_utf8 == 0)
		return base_utf8;

	return PathTraitsUTF8::Build(base_utf8.c_str(), uri_utf8);
}

AllocatedPath
LocalStorage::MapFS(const char *uri_utf8, Error &error) const
{
	assert(uri_utf8 != nullptr);

	if (*uri_utf8 == 0)
		return base_fs;

	AllocatedPath path_fs = AllocatedPath::FromUTF8(uri_utf8, error);
	if (!path_fs.IsNull())
		path_fs = AllocatedPath::Build(base_fs, path_fs);

	return path_fs;
}

AllocatedPath
LocalStorage::MapFS(const char *uri_utf8) const
{
	return MapFS(uri_utf8, IgnoreError());
}

const char *
LocalStorage::MapToRelativeUTF8(const char *uri_utf8) const
{
	return PathTraitsUTF8::Relative(base_utf8.c_str(), uri_utf8);
}

bool
LocalStorage::GetInfo(const char *uri_utf8, bool follow, FileInfo &info,
		      Error &error)
{
	AllocatedPath path_fs = MapFS(uri_utf8, error);
	if (path_fs.IsNull())
		return false;

	return Stat(path_fs, follow, info, error);
}

StorageDirectoryReader *
LocalStorage::OpenDirectory(const char *uri_utf8, Error &error)
{
	AllocatedPath path_fs = MapFS(uri_utf8, error);
	if (path_fs.IsNull())
		return nullptr;

	LocalDirectoryReader *reader =
		new LocalDirectoryReader(std::move(path_fs));
	if (reader->HasFailed()) {
		error.FormatErrno("Failed to open '%s'", uri_utf8);
		delete reader;
		return nullptr;
	}

	return reader;
}

gcc_pure
static bool
SkipNameFS(const char *name_fs)
{
	return name_fs[0] == '.' &&
		(name_fs[1] == 0 ||
		 (name_fs[1] == '.' && name_fs[2] == 0));
}

const char *
LocalDirectoryReader::Read()
{
	while (reader.ReadEntry()) {
		const Path name_fs = reader.GetEntry();
		if (SkipNameFS(name_fs.c_str()))
			continue;

		name_utf8 = name_fs.ToUTF8();
		if (name_utf8.empty())
			continue;

		return name_utf8.c_str();
	}

	return nullptr;
}

bool
LocalDirectoryReader::GetInfo(bool follow, FileInfo &info, Error &error)
{
	const AllocatedPath path_fs =
		AllocatedPath::Build(base_fs, reader.GetEntry());
	return Stat(path_fs, follow, info, error);
}

Storage *
CreateLocalStorage(const char *base_utf8, Path base_fs)
{
	return new LocalStorage(base_utf8, base_fs);
}
