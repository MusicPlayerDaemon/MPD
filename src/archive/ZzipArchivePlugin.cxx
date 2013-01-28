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
#include "ArchiveInternal.hxx"
#include "ArchivePlugin.hxx"
#include "InputInternal.hxx"
#include "InputStream.hxx"
#include "InputPlugin.hxx"
#include "refcount.h"

#include <zzip/zzip.h>
#include <glib.h>
#include <string.h>

struct ZzipArchiveFile {
	struct archive_file base;

	struct refcount ref;

	ZZIP_DIR *dir;
	GSList	*list;
	GSList	*iter;

	ZzipArchiveFile() {
		archive_file_init(&base, &zzip_archive_plugin);
		refcount_init(&ref);
	}

	void Unref() {
		if (!refcount_dec(&ref))
			return;

		if (list) {
			//free list
			for (GSList *tmp = list; tmp != NULL; tmp = g_slist_next(tmp))
				g_free(tmp->data);
			g_slist_free(list);
		}
		//close archive
		zzip_dir_close (dir);

		delete this;
	}
};

extern const struct input_plugin zzip_input_plugin;

static inline GQuark
zzip_quark(void)
{
	return g_quark_from_static_string("zzip");
}

/* archive open && listing routine */

static struct archive_file *
zzip_archive_open(const char *pathname, GError **error_r)
{
	ZzipArchiveFile *context = new ZzipArchiveFile();
	ZZIP_DIRENT dirent;

	// open archive
	context->list = NULL;
	context->dir = zzip_dir_open(pathname, NULL);
	if (context->dir  == NULL) {
		g_set_error(error_r, zzip_quark(), 0,
			    "Failed to open ZIP file %s", pathname);
		return NULL;
	}

	while (zzip_dir_read(context->dir, &dirent)) {
		//add only files
		if (dirent.st_size > 0) {
			context->list = g_slist_prepend(context->list,
							g_strdup(dirent.d_name));
		}
	}

	return &context->base;
}

static void
zzip_archive_scan_reset(struct archive_file *file)
{
	ZzipArchiveFile *context = (ZzipArchiveFile *) file;
	//reset iterator
	context->iter = context->list;
}

static char *
zzip_archive_scan_next(struct archive_file *file)
{
	ZzipArchiveFile *context = (ZzipArchiveFile *) file;
	char *data = NULL;
	if (context->iter != NULL) {
		///fetch data and goto next
		data = (char *)context->iter->data;
		context->iter = g_slist_next(context->iter);
	}
	return data;
}

static void
zzip_archive_close(struct archive_file *file)
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
		:archive(&_archive), file(_file) {
		input_stream_init(&base, &zzip_input_plugin, uri,
				  mutex, cond);

		base.ready = true;
		//we are seekable (but its not recommendent to do so)
		base.seekable = true;

		ZZIP_STAT z_stat;
		zzip_file_stat(file, &z_stat);
		base.size = z_stat.st_size;

		refcount_inc(&archive->ref);
	}

	~ZzipInputStream() {
		zzip_file_close(file);
		archive->Unref();
		input_stream_deinit(&base);
	}
};

static struct input_stream *
zzip_archive_open_stream(struct archive_file *file,
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
	zzip_archive_scan_reset,
	zzip_archive_scan_next,
	zzip_archive_open_stream,
	zzip_archive_close,
	zzip_archive_extensions,
};
