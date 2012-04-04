/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "archive/bz2_archive_plugin.h"
#include "archive_api.h"
#include "input_internal.h"
#include "input_plugin.h"
#include "refcount.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <glib.h>
#include <bzlib.h>

#ifdef HAVE_OLDER_BZIP2
#define BZ2_bzDecompressInit bzDecompressInit
#define BZ2_bzDecompress bzDecompress
#endif

struct bz2_archive_file {
	struct archive_file base;

	struct refcount ref;

	char *name;
	bool reset;
	struct input_stream *istream;
};

struct bz2_input_stream {
	struct input_stream base;

	struct bz2_archive_file *archive;

	bool eof;

	bz_stream bzstream;

	char buffer[5000];
};

static const struct input_plugin bz2_inputplugin;

static inline GQuark
bz2_quark(void)
{
	return g_quark_from_static_string("bz2");
}

/* single archive handling allocation helpers */

static bool
bz2_alloc(struct bz2_input_stream *data, GError **error_r)
{
	int ret;

	data->bzstream.bzalloc = NULL;
	data->bzstream.bzfree  = NULL;
	data->bzstream.opaque  = NULL;

	data->bzstream.next_in = (void *) data->buffer;
	data->bzstream.avail_in = 0;

	ret = BZ2_bzDecompressInit(&data->bzstream, 0, 0);
	if (ret != BZ_OK) {
		g_free(data);

		g_set_error(error_r, bz2_quark(), ret,
			    "BZ2_bzDecompressInit() has failed");
		return false;
	}

	return true;
}

static void
bz2_destroy(struct bz2_input_stream *data)
{
	BZ2_bzDecompressEnd(&data->bzstream);
}

/* archive open && listing routine */

#if GCC_CHECK_VERSION(4, 2)
/* workaround for a warning caused by G_STATIC_MUTEX_INIT */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

static struct archive_file *
bz2_open(const char *pathname, GError **error_r)
{
	struct bz2_archive_file *context;
	int len;

	context = g_malloc(sizeof(*context));
	archive_file_init(&context->base, &bz2_archive_plugin);
	refcount_init(&context->ref);

	//open archive
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
	context->istream = input_stream_open(pathname,
					     g_static_mutex_get_mutex(&mutex),
					     NULL,
					     error_r);
	if (context->istream == NULL) {
		g_free(context);
		return NULL;
	}

	context->name = g_path_get_basename(pathname);

	//remove suffix
	len = strlen(context->name);
	if (len > 4) {
		context->name[len - 4] = 0; //remove .bz2 suffix
	}

	return &context->base;
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

	if (!refcount_dec(&context->ref))
		return;

	g_free(context->name);

	input_stream_close(context->istream);
	g_free(context);
}

/* single archive handling */

static struct input_stream *
bz2_open_stream(struct archive_file *file, const char *path,
		GMutex *mutex, GCond *cond,
		GError **error_r)
{
	struct bz2_archive_file *context = (struct bz2_archive_file *) file;
	struct bz2_input_stream *bis = g_new(struct bz2_input_stream, 1);

	input_stream_init(&bis->base, &bz2_inputplugin, path,
			  mutex, cond);

	bis->archive = context;

	bis->base.ready = true;
	bis->base.seekable = false;

	if (!bz2_alloc(bis, error_r)) {
		input_stream_deinit(&bis->base);
		g_free(bis);
		return NULL;
	}

	bis->eof = false;

	refcount_inc(&context->ref);

	return &bis->base;
}

static void
bz2_is_close(struct input_stream *is)
{
	struct bz2_input_stream *bis = (struct bz2_input_stream *)is;

	bz2_destroy(bis);

	bz2_close(&bis->archive->base);

	input_stream_deinit(&bis->base);
	g_free(bis);
}

static bool
bz2_fillbuffer(struct bz2_input_stream *bis, GError **error_r)
{
	size_t count;
	bz_stream *bzstream;

	bzstream = &bis->bzstream;

	if (bzstream->avail_in > 0)
		return true;

	count = input_stream_read(bis->archive->istream,
				  bis->buffer, sizeof(bis->buffer),
				  error_r);
	if (count == 0)
		return false;

	bzstream->next_in = bis->buffer;
	bzstream->avail_in = count;
	return true;
}

static size_t
bz2_is_read(struct input_stream *is, void *ptr, size_t length,
	    GError **error_r)
{
	struct bz2_input_stream *bis = (struct bz2_input_stream *)is;
	bz_stream *bzstream;
	int bz_result;
	size_t nbytes = 0;

	if (bis->eof)
		return 0;

	bzstream = &bis->bzstream;
	bzstream->next_out = ptr;
	bzstream->avail_out = length;

	do {
		if (!bz2_fillbuffer(bis, error_r))
			return 0;

		bz_result = BZ2_bzDecompress(bzstream);

		if (bz_result == BZ_STREAM_END) {
			bis->eof = true;
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
	struct bz2_input_stream *bis = (struct bz2_input_stream *)is;

	return bis->eof;
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

