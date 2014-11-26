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

/**
  * zip archive handling (requires zziplib)
  */

#include "config.h"
#include "ZzipArchivePlugin.hxx"
#include "../ArchivePlugin.hxx"
#include "../ArchiveFile.hxx"
#include "../ArchiveVisitor.hxx"
#include "input/InputStream.hxx"
#include "input/InputPlugin.hxx"
#include "fs/Path.hxx"
#include "util/RefCount.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <zzip/zzip.h>

class ZzipArchiveFile final : public ArchiveFile {
public:
	RefCount ref;

	ZZIP_DIR *const dir;

	ZzipArchiveFile(ZZIP_DIR *_dir)
		:ArchiveFile(zzip_archive_plugin), dir(_dir) {}

	~ZzipArchiveFile() {
		zzip_dir_close(dir);
	}

	void Unref() {
		if (ref.Decrement())
			delete this;
	}

	virtual void Close() override {
		Unref();
	}

	virtual void Visit(ArchiveVisitor &visitor) override;

	virtual InputStream *OpenStream(const char *path,
					Mutex &mutex, Cond &cond,
					Error &error) override;
};

static constexpr Domain zzip_domain("zzip");

/* archive open && listing routine */

static ArchiveFile *
zzip_archive_open(Path pathname, Error &error)
{
	ZZIP_DIR *dir = zzip_dir_open(pathname.c_str(), nullptr);
	if (dir == nullptr) {
		error.Format(zzip_domain, "Failed to open ZIP file %s",
			     pathname.c_str());
		return nullptr;
	}

	return new ZzipArchiveFile(dir);
}

inline void
ZzipArchiveFile::Visit(ArchiveVisitor &visitor)
{
	zzip_rewinddir(dir);

	ZZIP_DIRENT dirent;
	while (zzip_dir_read(dir, &dirent))
		//add only files
		if (dirent.st_size > 0)
			visitor.VisitArchiveEntry(dirent.d_name);
}

/* single archive handling */

struct ZzipInputStream final : public InputStream {
	ZzipArchiveFile *archive;

	ZZIP_FILE *file;

	ZzipInputStream(ZzipArchiveFile &_archive, const char *_uri,
			Mutex &_mutex, Cond &_cond,
			ZZIP_FILE *_file)
		:InputStream(_uri, _mutex, _cond),
		 archive(&_archive), file(_file) {
		//we are seekable (but its not recommendent to do so)
		seekable = true;

		ZZIP_STAT z_stat;
		zzip_file_stat(file, &z_stat);
		size = z_stat.st_size;

		SetReady();

		archive->ref.Increment();
	}

	~ZzipInputStream() {
		zzip_file_close(file);
		archive->Unref();
	}

	/* virtual methods from InputStream */
	bool IsEOF() override;
	size_t Read(void *ptr, size_t size, Error &error) override;
	bool Seek(offset_type offset, Error &error) override;
};

InputStream *
ZzipArchiveFile::OpenStream(const char *pathname,
			    Mutex &mutex, Cond &cond,
			    Error &error)
{
	ZZIP_FILE *_file = zzip_file_open(dir, pathname, 0);
	if (_file == nullptr) {
		error.Format(zzip_domain, "not found in the ZIP file: %s",
			     pathname);
		return nullptr;
	}

	return new ZzipInputStream(*this, pathname,
				   mutex, cond,
				   _file);
}

size_t
ZzipInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	int ret = zzip_file_read(file, ptr, read_size);
	if (ret < 0) {
		error.Set(zzip_domain, "zzip_file_read() has failed");
		return 0;
	}

	offset = zzip_tell(file);
	return ret;
}

bool
ZzipInputStream::IsEOF()
{
	return offset_type(zzip_tell(file)) == size;
}

bool
ZzipInputStream::Seek(offset_type new_offset, Error &error)
{
	zzip_off_t ofs = zzip_seek(file, new_offset, SEEK_SET);
	if (ofs < 0) {
		error.Set(zzip_domain, "zzip_seek() has failed");
		return false;
	}

	offset = ofs;
	return true;
}

/* exported structures */

static const char *const zzip_archive_extensions[] = {
	"zip",
	nullptr
};

const ArchivePlugin zzip_archive_plugin = {
	"zzip",
	nullptr,
	nullptr,
	zzip_archive_open,
	zzip_archive_extensions,
};
