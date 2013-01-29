/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "InputInternal.hxx"
#include "InputStream.hxx"
#include "InputPlugin.hxx"
#include "util/RefCount.hxx"

#include <zzip/zzip.h>
#include <glib.h>
#include <string.h>

class ZzipArchiveFile : public ArchiveFile {
public:
	RefCount ref;

	ZZIP_DIR *dir;

	ZzipArchiveFile():ArchiveFile(zzip_archive_plugin) {}

	void Unref() {
		if (!ref.Decrement())
			return;

		//close archive
		zzip_dir_close (dir);

		delete this;
	}

	void Visit(ArchiveVisitor &visitor);
};

extern const struct input_plugin zzip_input_plugin;

static inline GQuark
zzip_quark(void)
{
	return g_quark_from_static_string("zzip");
}

/* archive open && listing routine */

static ArchiveFile *
zzip_archive_open(const char *pathname, GError **error_r)
{
	ZzipArchiveFile *context = new ZzipArchiveFile();

	// open archive
	context->dir = zzip_dir_open(pathname, NULL);
	if (context->dir  == NULL) {
		g_set_error(error_r, zzip_quark(), 0,
			    "Failed to open ZIP file %s", pathname);
		return NULL;
	}

	return context;
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

static void
zzip_archive_visit(ArchiveFile *file, ArchiveVisitor &visitor)
{
	ZzipArchiveFile *context = (ZzipArchiveFile *) file;

	context->Visit(visitor);
}

static void
zzip_archive_close(ArchiveFile *file)
{
	ZzipArchiveFile *context = (ZzipArchiveFile *) file;

	context->Unref();
}

/* single archive handling */

struct ZzipInputStream {
	struct input_stream base;

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

static struct input_stream *
zzip_archive_open_stream(ArchiveFile *file,
			 const char *pathname,
			 Mutex &mutex, Cond &cond,
			 GError **error_r)
{
	ZzipArchiveFile *context = (ZzipArchiveFile *) file;

	ZZIP_FILE *_file = zzip_file_open(context->dir, pathname, 0);
	if (_file == nullptr) {
		g_set_error(error_r, zzip_quark(), 0,
			    "not found in the ZIP file: %s", pathname);
		return NULL;
	}

	ZzipInputStream *zis =
		new ZzipInputStream(*context, pathname,
				    mutex, cond,
				    _file);
	return &zis->base;
}

static void
zzip_input_close(struct input_stream *is)
{
	ZzipInputStream *zis = (ZzipInputStream *)is;

	delete zis;
}

static size_t
zzip_input_read(struct input_stream *is, void *ptr, size_t size,
		GError **error_r)
{
	ZzipInputStream *zis = (ZzipInputStream *)is;
	int ret;

	ret = zzip_file_read(zis->file, ptr, size);
	if (ret < 0) {
		g_set_error(error_r, zzip_quark(), ret,
			    "zzip_file_read() has failed");
		return 0;
	}

	is->offset = zzip_tell(zis->file);

	return ret;
}

static bool
zzip_input_eof(struct input_stream *is)
{
	ZzipInputStream *zis = (ZzipInputStream *)is;

	return (goffset)zzip_tell(zis->file) == is->size;
}

static bool
zzip_input_seek(struct input_stream *is,
		goffset offset, int whence, GError **error_r)
{
	ZzipInputStream *zis = (ZzipInputStream *)is;
	zzip_off_t ofs = zzip_seek(zis->file, offset, whence);
	if (ofs != -1) {
		g_set_error(error_r, zzip_quark(), ofs,
			    "zzip_seek() has failed");
		is->offset = ofs;
		return true;
	}
	return false;
}

/* exported structures */

static const char *const zzip_archive_extensions[] = {
	"zip",
	NULL
};

const struct input_plugin zzip_input_plugin = {
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
	zzip_archive_visit,
	zzip_archive_open_stream,
	zzip_archive_close,
	zzip_archive_extensions,
};
