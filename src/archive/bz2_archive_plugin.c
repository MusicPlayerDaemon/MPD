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
#define BZ2_bzDecompressInit bzDecompressInit
#define BZ2_bzDecompress bzDecompress
#endif

#define BZ_BUFSIZE   5000

struct bz2_archive_file {
	char *name;
	bool reset;
	struct input_stream istream;

	bool eof;

	bz_stream bzstream;
	char *buffer;
};

static const struct input_plugin bz2_inputplugin;

static inline GQuark
bz2_quark(void)
{
	return g_quark_from_static_string("bz2");
}

/* single archive handling allocation helpers */

static bool
bz2_alloc(struct bz2_archive_file *data, GError **error_r)
{
	int ret;

	data->bzstream.bzalloc = NULL;
	data->bzstream.bzfree  = NULL;
	data->bzstream.opaque  = NULL;

	data->buffer = g_malloc(BZ_BUFSIZE);
	data->bzstream.next_in = (void *) data->buffer;
	data->bzstream.avail_in = 0;

	ret = BZ2_bzDecompressInit(&data->bzstream, 0, 0);
	if (ret != BZ_OK) {
		g_free(data->buffer);
		g_free(data);

		g_set_error(error_r, bz2_quark(), ret,
			    "BZ2_bzDecompressInit() has failed");
		return false;
	}

	return true;
}

static void
bz2_destroy(struct bz2_archive_file *data)
{
	BZ2_bzDecompressEnd(&data->bzstream);
	g_free(data->buffer);
}

/* archive open && listing routine */

static struct archive_file *
bz2_open(char *pathname)
{
	struct bz2_archive_file *context;
	int len;

	context = g_malloc(sizeof(*context));

	//open archive
	if (!input_stream_open(&context->istream, pathname, NULL)) {
		g_warning("failed to open an bzip2 archive %s\n",pathname);
		g_free(context);
		return NULL;
	}

	context->name = g_path_get_basename(pathname);

	//remove suffix
	len = strlen(context->name);
	if (len > 4) {
		context->name[len - 4] = 0; //remove .bz2 suffix
	}

	return (struct archive_file *) context;
}

static void
bz2_scan_reset(struct archive_file *file)
{
	struct bz2_archive_file *context = (struct bz2_archive_file *) file;
	context->reset = true;
}

static char *
bz2_scan_next(struct archive_file *file)
{
	struct bz2_archive_file *context = (struct bz2_archive_file *) file;
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
	struct bz2_archive_file *context = (struct bz2_archive_file *) file;

	g_free(context->name);

	input_stream_close(&context->istream);
	g_free(context);
}

/* single archive handling */

static bool
bz2_open_stream(struct archive_file *file, struct input_stream *is,
		G_GNUC_UNUSED const char *path, GError **error_r)
{
	struct bz2_archive_file *context = (struct bz2_archive_file *) file;

	//setup file ops
	is->plugin = &bz2_inputplugin;
	//insert back reference
	is->data = context;
	is->seekable = false;

	if (!bz2_alloc(context, error_r))
		return false;

	context->eof = false;

	return true;
}

static void
bz2_is_close(struct input_stream *is)
{
	struct bz2_archive_file *context = (struct bz2_archive_file *) is->data;
	bz2_destroy(context);
	is->data = NULL;

	bz2_close((struct archive_file *)context);
}

static bool
bz2_fillbuffer(struct bz2_archive_file *context, GError **error_r)
{
	size_t count;
	bz_stream *bzstream;

	bzstream = &context->bzstream;

	if (bzstream->avail_in > 0)
		return true;

	count = input_stream_read(&context->istream,
				  context->buffer, BZ_BUFSIZE,
				  error_r);
	if (count == 0)
		return false;

	bzstream->next_in = context->buffer;
	bzstream->avail_in = count;
	return true;
}

static size_t
bz2_is_read(struct input_stream *is, void *ptr, size_t length,
	    GError **error_r)
{
	struct bz2_archive_file *context = (struct bz2_archive_file *) is->data;
	bz_stream *bzstream;
	int bz_result;
	size_t nbytes = 0;

	if (context->eof)
		return 0;

	bzstream = &context->bzstream;
	bzstream->next_out = ptr;
	bzstream->avail_out = length;

	do {
		if (!bz2_fillbuffer(context, error_r))
			return 0;

		bz_result = BZ2_bzDecompress(bzstream);

		if (bz_result == BZ_STREAM_END) {
			context->eof = true;
			break;
		}

		if (bz_result != BZ_OK) {
			g_set_error(error_r, bz2_quark(), bz_result,
				    "BZ2_bzDecompress() has failed");
			return 0;
		}
	} while (bzstream->avail_out == length);

	nbytes = length - bzstream->avail_out;
	is->offset += nbytes;

	return nbytes;
}

static bool
bz2_is_eof(struct input_stream *is)
{
	struct bz2_archive_file *context = (struct bz2_archive_file *) is->data;

	return context->eof;
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

const struct archive_plugin bz2_archive_plugin = {
	.name = "bz2",
	.open = bz2_open,
	.scan_reset = bz2_scan_reset,
	.scan_next = bz2_scan_next,
	.open_stream = bz2_open_stream,
	.close = bz2_close,
	.suffixes = bz2_extensions
};

