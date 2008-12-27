/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Viliam Mateicka <viliam.mateicka@gmail.com>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
  * zip archive handling (requires zziplib)
  */

#include "archive_api.h"
#include "archive_api.h"
#include "input_stream.h"
#include "utils.h"

#include <zzip/zzip.h>
#include <glib.h>
#include <string.h>

typedef struct {
	ZZIP_DIR *dir;
	ZZIP_FILE *file;
	size_t	length;
	GSList	*list;
	GSList	*iter;
} zip_context;

static const struct input_plugin zip_inputplugin;

/* archive open && listing routine */

static struct archive_file *
zip_open(char * pathname)
{
	zip_context *context = g_malloc(sizeof(zip_context));
	ZZIP_DIRENT dirent;

	// open archive
	context->list = NULL;
	context->dir = zzip_dir_open(pathname, 0);
	if (context->dir  == NULL) {
		g_warning("zipfile %s open failed\n", pathname);
		return NULL;
	}

	while (zzip_dir_read(context->dir, &dirent)) {
		context->list = g_slist_prepend( context->list, xstrdup(dirent.d_name));
	}

	return (struct archive_file *)context;
}

static void
zip_scan_reset(struct archive_file *file)
{
	zip_context *context = (zip_context *) file;
	//reset iterator
	context->iter = context->list;
}

static char *
zip_scan_next(struct archive_file *file)
{
	zip_context *context = (zip_context *) file;
	char *data = NULL;
	if (context->iter != NULL) {
		///fetch data and goto next
		data = context->iter->data;
		context->iter = g_slist_next(context->iter);
	}
	return data;
}

static void
zip_close(struct archive_file *file)
{
	zip_context *context = (zip_context *) file;
	if (context->list) {
		//free list
		for (GSList *tmp = context->list; tmp != NULL; tmp = g_slist_next(tmp))
			g_free(tmp->data);
		g_slist_free(context->list);
	}
	//close archive
	zzip_dir_close (context->dir);
	context->dir = NULL;
}

/* single archive handling */

static void
zip_setup_stream(struct archive_file *file, struct input_stream *is)
{
	zip_context *context = (zip_context *) file;
	//setup file ops
	is->plugin = &zip_inputplugin;
	//insert back reference
	is->archive = context;
	//we are not seekable
	is->seekable = false;
}


static bool
zip_is_open(struct input_stream *is, const char *pathname)
{
	zip_context *context = (zip_context *) is->archive;
	ZZIP_STAT z_stat;

	context->file = zzip_file_open(context->dir, pathname, 0);
	if (!context->file) {
		g_warning("file %s not found in the zipfile\n", pathname);
		return false;
	}
	zzip_file_stat(context->file, &z_stat);
	context->length = z_stat.st_size;
	return true;
}

static void
zip_is_close(struct input_stream *is)
{
	zip_context *context = (zip_context *) is->archive;
	zzip_file_close (context->file);
}

static size_t
zip_is_read(struct input_stream *is, void *ptr, size_t size)
{
	zip_context *context = (zip_context *) is->archive;
	int ret;
	ret = zzip_file_read(context->file, ptr, size);
	if (ret < 0) {
		g_warning("error %d reading zipfile\n", ret);
		return 0;
	}
	return ret;
}

static bool
zip_is_eof(struct input_stream *is)
{
	zip_context *context = (zip_context *) is->archive;
	return ((size_t) zzip_tell(context->file) == context->length);
}

static bool
zip_is_seek(G_GNUC_UNUSED struct input_stream *is,
	G_GNUC_UNUSED off_t offset, G_GNUC_UNUSED int whence)
{
	return false;
}

static int
zip_is_buffer(G_GNUC_UNUSED struct input_stream *is)
{
	return 0;
}

/* exported structures */

static const char *const zip_extensions[] = {
	"zip",
	NULL
};

static const struct input_plugin zip_inputplugin = {
	.open = zip_is_open,
	.close = zip_is_close,
	.read = zip_is_read,
	.eof = zip_is_eof,
	.seek = zip_is_seek,
	.buffer = zip_is_buffer
};

const struct archive_plugin zip_plugin = {
	.name = "zip",
	.open = zip_open,
	.scan_reset = zip_scan_reset,
	.scan_next = zip_scan_next,
	.setup_stream = zip_setup_stream,
	.close = zip_close,
	.suffixes = zip_extensions
};
