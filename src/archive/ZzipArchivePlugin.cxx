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
#include "ArchivePlugin.hxx"
#include "ArchiveFile.hxx"
#include "ArchiveVisitor.hxx"
#include "InputStream.hxx"
#include "InputPlugin.hxx"
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

extern const InputPlugin zzip_input_plugin;

static constexpr Domain zzip_domain("zzip");

/* archive open && listing routine */

static ArchiveFile *
zzip_archive_open(const char *pathname, Error &error)
{
	ZZIP_DIR *dir = zzip_dir_open(pathname, nullptr);
	if (dir == nullptr) {
		error.Format(zzip_domain, "Failed to open ZIP file %s",
			     pathname);
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

struct ZzipInputStream {
	InputStream base;

	ZzipArchiveFile *archive;

	ZZIP_FILE *file;

	ZzipInputStream(ZzipArchiveFile &_archive, const char *uri,
			Mutex &mutex, Cond &cond,
			ZZIP_FILE *_file)
		:base(zzip_input_plugin, uri, mutex, cond),
		 archive(&_archive), file(_file) {
		base.ready = true;
		//we are seekable (but its not recommendent to do so)
		base.seekable = true;

		ZZIP_STAT z_stat;
		zzip_file_stat(file, &z_stat);
		base.size = z_stat.st_size;

		archive->ref.Increment();
	}

	~ZzipInputStream() {
		zzip_file_close(file);
		archive->Unref();
	}
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

	ZzipInputStream *zis =
		new ZzipInputStream(*this, pathname,
				    mutex, cond,
				    _file);
	return &zis->base;
}

static void
zzip_input_close(InputStream *is)
{
	ZzipInputStream *zis = (ZzipInputStream *)is;

	delete zis;
}

static size_t
zzip_input_read(InputStream *is, void *ptr, size_t size,
		Error &error)
{
	ZzipInputStream *zis = (ZzipInputStream *)is;
	int ret;

	ret = zzip_file_read(zis->file, ptr, size);
	if (ret < 0) {
		error.Set(zzip_domain, "zzip_file_read() has failed");
		return 0;
	}

	is->offset = zzip_tell(zis->file);

	return ret;
}

static bool
zzip_input_eof(InputStream *is)
{
	ZzipInputStream *zis = (ZzipInputStream *)is;

	return (InputPlugin::offset_type)zzip_tell(zis->file) == is->size;
}

static bool
zzip_input_seek(InputStream *is, InputPlugin::offset_type offset,
		int whence, Error &error)
{
	ZzipInputStream *zis = (ZzipInputStream *)is;
	zzip_off_t ofs = zzip_seek(zis->file, offset, whence);
	if (ofs != -1) {
		error.Set(zzip_domain, "zzip_seek() has failed");
		is->offset = ofs;
		return true;
	}
	return false;
}

/* exported structures */

static const char *const zzip_archive_extensions[] = {
	"zip",
	nullptr
};

const InputPlugin zzip_input_plugin = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	zzip_input_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	zzip_input_read,
	zzip_input_eof,
	zzip_input_seek,
};

const struct archive_plugin zzip_archive_plugin = {
	"zzip",
	nullptr,
	nullptr,
	zzip_archive_open,
	zzip_archive_extensions,
};
