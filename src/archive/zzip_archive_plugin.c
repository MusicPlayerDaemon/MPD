/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "archive_api.h"
#include "archive_api.h"
#include "input_plugin.h"

#include <zzip/zzip.h>
#include <glib.h>
#include <string.h>

struct zzip_archive {
	ZZIP_DIR *dir;
	ZZIP_FILE *file;
	size_t	length;
	GSList	*list;
	GSList	*iter;
};

static const struct input_plugin zzip_input_plugin;

static inline GQuark
zzip_quark(void)
{
	return g_quark_from_static_string("zzip");
}

/* archive open && listing routine */

static struct archive_file *
zzip_archive_open(char *pathname)
{
	struct zzip_archive *context = g_malloc(sizeof(*context));
	ZZIP_DIRENT dirent;

	// open archive
	context->list = NULL;
	context->dir = zzip_dir_open(pathname, NULL);
	if (context->dir  == NULL) {
		g_warning("zipfile %s open failed\n", pathname);
		return NULL;
	}

	while (zzip_dir_read(context->dir, &dirent)) {
		//add only files
		if (dirent.st_size > 0) {
			context->list = g_slist_prepend(context->list,
							g_strdup(dirent.d_name));
		}
	}

	return (struct archive_file *)context;
}

static void
zzip_archive_scan_reset(struct archive_file *file)
{
	struct zzip_archive *context = (struct zzip_archive *) file;
	//reset iterator
	context->iter = context->list;
}

static char *
zzip_archive_scan_next(struct archive_file *file)
{
	struct zzip_archive *context = (struct zzip_archive *) file;
	char *data = NULL;
	if (context->iter != NULL) {
		///fetch data and goto next
		data = context->iter->data;
		context->iter = g_slist_next(context->iter);
	}
	return data;
}

static void
zzip_archive_close(struct archive_file *file)
{
	struct zzip_archive *context = (struct zzip_archive *) file;
	if (context->list) {
		//free list
		for (GSList *tmp = context->list; tmp != NULL; tmp = g_slist_next(tmp))
			g_free(tmp->data);
		g_slist_free(context->list);
	}
	//close archive
	zzip_dir_close (context->dir);

	g_free(context);
}

/* single archive handling */

static bool
zzip_archive_open_stream(struct archive_file *file, struct input_stream *is,
			 const char *pathname, GError **error_r)
{
	struct zzip_archive *context = (struct zzip_archive *) file;
	ZZIP_STAT z_stat;

	//setup file ops
	is->plugin = &zzip_input_plugin;
	//insert back reference
	is->data = context;
	//we are seekable (but its not recommendent to do so)
	is->seekable = true;

	context->file = zzip_file_open(context->dir, pathname, 0);
	if (!context->file) {
		g_set_error(error_r, zzip_quark(), 0,
			    "not found in the ZIP file: %s", pathname);
		return false;
	}
	zzip_file_stat(context->file, &z_stat);
	context->length = z_stat.st_size;
	return true;
}

static void
zzip_input_close(struct input_stream *is)
{
	struct zzip_archive *context = (struct zzip_archive *) is->data;
	zzip_file_close (context->file);

	zzip_archive_close((struct archive_file *)context);
}

static size_t
zzip_input_read(struct input_stream *is, void *ptr, size_t size,
		GError **error_r)
{
	struct zzip_archive *context = (struct zzip_archive *) is->data;
	int ret;
	ret = zzip_file_read(context->file, ptr, size);
	if (ret < 0) {
		g_set_error(error_r, zzip_quark(), ret,
			    "zzip_file_read() has failed");
		return 0;
	}
	return ret;
}

static bool
zzip_input_eof(struct input_stream *is)
{
	struct zzip_archive *context = (struct zzip_archive *) is->data;
	return ((size_t) zzip_tell(context->file) == context->length);
}

static bool
zzip_input_seek(struct input_stream *is,
		goffset offset, int whence, GError **error_r)
{
	struct zzip_archive *context = (struct zzip_archive *) is->data;
	zzip_off_t ofs = zzip_seek(context->file, offset, whence);
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

static const struct input_plugin zzip_input_plugin = {
	.close = zzip_input_close,
	.read = zzip_input_read,
	.eof = zzip_input_eof,
	.seek = zzip_input_seek,
};

const struct archive_plugin zzip_archive_plugin = {
	.name = "zzip",
	.open = zzip_archive_open,
	.scan_reset = zzip_archive_scan_reset,
	.scan_next = zzip_archive_scan_next,
	.open_stream = zzip_archive_open_stream,
	.close = zzip_archive_close,
	.suffixes = zzip_archive_extensions
};
