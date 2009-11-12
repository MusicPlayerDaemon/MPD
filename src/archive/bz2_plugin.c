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
  * single bz2 archive handling (requires libbz2)
  */

#include "config.h"
#include "archive_api.h"
#include "input_plugin.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <glib.h>
#include <bzlib.h>

#ifdef HAVE_OLDER_BZIP2
#define BZ2_bzDecompressInit  bzDecompressInit
#define BZ2_bzDecompress      bzDecompress
#endif

#define BZ_BUFSIZE   5000

typedef struct {
	char		*name;
	bool		reset;
	struct input_stream istream;
	int		last_bz_result;
	int		last_parent_result;
	bz_stream	bzstream;
	char		*buffer;
} bz2_context;


static const struct input_plugin bz2_inputplugin;

/* single archive handling allocation helpers */

static bool
bz2_alloc(bz2_context *data)
{
	data->bzstream.bzalloc = NULL;
	data->bzstream.bzfree  = NULL;
	data->bzstream.opaque  = NULL;

	data->buffer = g_malloc(BZ_BUFSIZE);
	data->bzstream.next_in = (void *) data->buffer;
	data->bzstream.avail_in = 0;

	if (BZ2_bzDecompressInit(&data->bzstream, 0, 0) != BZ_OK) {
		g_free(data->buffer);
		g_free(data);
		return false;
	}

	data->last_bz_result = BZ_OK;
	data->last_parent_result = 0;
	return true;
}

static void
bz2_destroy(bz2_context *data)
{
	BZ2_bzDecompressEnd(&data->bzstream);
	g_free(data->buffer);
}

/* archive open && listing routine */

static struct archive_file *
bz2_open(char * pathname)
{
	bz2_context *context;
	char *name;
	int len;

	context = g_malloc(sizeof(bz2_context));
	if (!context) {
		return NULL;
	}
	//open archive
	if (!input_stream_open(&context->istream, pathname)) {
		g_warning("failed to open an bzip2 archive %s\n",pathname);
		g_free(context);
		return NULL;
	}
	//capture filename
	name = strrchr(pathname, '/');
	if (name == NULL) {
		g_warning("failed to get bzip2 name from %s\n",pathname);
		g_free(context);
		return NULL;
	}
	context->name = g_strdup(name+1);
	//remove suffix
	len = strlen(context->name);
	if (len > 4) {
		context->name[len-4] = 0; //remove .bz2 suffix
	}
	return (struct archive_file *) context;
}

static void
bz2_scan_reset(struct archive_file *file)
{
	bz2_context *context = (bz2_context *) file;
	context->reset = true;
}

static char *
bz2_scan_next(struct archive_file *file)
{
	bz2_context *context = (bz2_context *) file;
	char *name = NULL;
	if (context->reset) {
		name = context->name;
		context->reset = false;
	}
	return name;
}

static void
bz2_close(struct archive_file *file)
{
	bz2_context *context = (bz2_context *) file;
	if (context->name)
		g_free(context->name);

	input_stream_close(&context->istream);
	g_free(context);
}

/* single archive handling */

static bool
bz2_open_stream(struct archive_file *file, struct input_stream *is,
		G_GNUC_UNUSED const char *path)
{
	bz2_context *context = (bz2_context *) file;
	//setup file ops
	is->plugin = &bz2_inputplugin;
	//insert back reference
	is->data = context;
	is->seekable = false;

	if (!bz2_alloc(context)) {
		g_warning("alloc bz2 failed\n");
		return false;
	}
	return true;
}

static void
bz2_is_close(struct input_stream *is)
{
	bz2_context *context = (bz2_context *) is->data;
	bz2_destroy(context);
	is->data = NULL;
}

static int
bz2_fillbuffer(bz2_context *context,
	size_t numBytes)
{
	size_t count;
	bz_stream *bzstream;

	bzstream = &context->bzstream;

	if (bzstream->avail_in > 0)
		return 0;

	count = input_stream_read(&context->istream,
			context->buffer, BZ_BUFSIZE);

	if (count == 0) {
		if (bzstream->avail_out == numBytes)
			return -1;
		if (!input_stream_eof(&context->istream))
			context->last_parent_result = 1;
	} else {
		bzstream->next_in = context->buffer;
		bzstream->avail_in = count;
	}

	return 0;
}

static size_t
bz2_is_read(struct input_stream *is, void *ptr, size_t size)
{
	bz2_context *context = (bz2_context *) is->data;
	bz_stream *bzstream;
	int bz_result;
	size_t numBytes = size;
	size_t bytesRead = 0;

	if (context->last_bz_result != BZ_OK)
		return 0;
	if (context->last_parent_result != 0)
		return 0;

	bzstream = &context->bzstream;
	bzstream->next_out = ptr;
	bzstream->avail_out = numBytes;

	while (bzstream->avail_out != 0) {
		if (bz2_fillbuffer(context, numBytes) != 0)
			break;

		bz_result = BZ2_bzDecompress(bzstream);

		if (context->last_bz_result != BZ_OK
		    && bzstream->avail_out == numBytes) {
			context->last_bz_result = bz_result;
			break;
		}

		if (bz_result == BZ_STREAM_END) {
			context->last_bz_result = bz_result;
			break;
		}
	}

	bytesRead = numBytes - bzstream->avail_out;
	is->offset += bytesRead;

	return bytesRead;
}

static bool
bz2_is_eof(struct input_stream *is)
{
	bz2_context *context = (bz2_context *) is->data;

	if (context->last_bz_result == BZ_STREAM_END) {
		return true;
	}

	return false;
}

/* exported structures */

static const char *const bz2_extensions[] = {
	"bz2",
	NULL
};

static const struct input_plugin bz2_inputplugin = {
	.close = bz2_is_close,
	.read = bz2_is_read,
	.eof = bz2_is_eof,
};

const struct archive_plugin bz2_plugin = {
	.name = "bz2",
	.open = bz2_open,
	.scan_reset = bz2_scan_reset,
	.scan_next = bz2_scan_next,
	.open_stream = bz2_open_stream,
	.close = bz2_close,
	.suffixes = bz2_extensions
};

