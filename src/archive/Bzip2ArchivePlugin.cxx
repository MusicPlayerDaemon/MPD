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
  * single bz2 archive handling (requires libbz2)
  */

#include "config.h"
#include "Bzip2ArchivePlugin.hxx"
#include "ArchiveInternal.hxx"
#include "ArchivePlugin.hxx"
#include "InputInternal.hxx"
#include "InputStream.hxx"
#include "InputPlugin.hxx"
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

struct Bzip2ArchiveFile {
	struct archive_file base;

	struct refcount ref;

	char *name;
	bool reset;
	struct input_stream *istream;

	Bzip2ArchiveFile() {
		archive_file_init(&base, &bz2_archive_plugin);
		refcount_init(&ref);
	}

	void Unref() {
		if (!refcount_dec(&ref))
			return;

		g_free(name);

		input_stream_close(istream);
		delete this;
	}
};

struct Bzip2InputStream {
	struct input_stream base;

	Bzip2ArchiveFile *archive;

	bool eof;

	bz_stream bzstream;

	char buffer[5000];

	Bzip2InputStream(Bzip2ArchiveFile &context, const char *uri,
			 Mutex &mutex, Cond &cond);
	~Bzip2InputStream();

	bool Open(GError **error_r);
	void Close();
};

extern const struct input_plugin bz2_inputplugin;

static inline GQuark
bz2_quark(void)
{
	return g_quark_from_static_string("bz2");
}

/* single archive handling allocation helpers */

inline bool
Bzip2InputStream::Open(GError **error_r)
{
	bzstream.bzalloc = nullptr;
	bzstream.bzfree = nullptr;
	bzstream.opaque = nullptr;

	bzstream.next_in = (char *)buffer;
	bzstream.avail_in = 0;

	int ret = BZ2_bzDecompressInit(&bzstream, 0, 0);
	if (ret != BZ_OK) {
		g_set_error(error_r, bz2_quark(), ret,
			    "BZ2_bzDecompressInit() has failed");
		return false;
	}

	base.ready = true;
	return true;
}

inline void
Bzip2InputStream::Close()
{
	BZ2_bzDecompressEnd(&bzstream);
}

/* archive open && listing routine */

static struct archive_file *
bz2_open(const char *pathname, GError **error_r)
{
	Bzip2ArchiveFile *context = new Bzip2ArchiveFile();
	int len;

	//open archive
	static Mutex mutex;
	static Cond cond;
	context->istream = input_stream_open(pathname, mutex, cond,
					     error_r);
	if (context->istream == NULL) {
		delete context;
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
	Bzip2ArchiveFile *context = (Bzip2ArchiveFile *) file;
	context->reset = true;
}

static char *
bz2_scan_next(struct archive_file *file)
{
	Bzip2ArchiveFile *context = (Bzip2ArchiveFile *) file;
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
	Bzip2ArchiveFile *context = (Bzip2ArchiveFile *) file;

	context->Unref();
}

/* single archive handling */

Bzip2InputStream::Bzip2InputStream(Bzip2ArchiveFile &_context, const char *uri,
				   Mutex &mutex, Cond &cond)
	:archive(&_context), eof(false)
{
	input_stream_init(&base, &bz2_inputplugin, uri, mutex, cond);
	refcount_inc(&archive->ref);
}

Bzip2InputStream::~Bzip2InputStream()
{
	bz2_close(&archive->base);
	input_stream_deinit(&base);
}

static struct input_stream *
bz2_open_stream(struct archive_file *file, const char *path,
		Mutex &mutex, Cond &cond,
		GError **error_r)
{
	Bzip2ArchiveFile *context = (Bzip2ArchiveFile *) file;
	Bzip2InputStream *bis =
		new Bzip2InputStream(*context, path, mutex, cond);

	if (!bis->Open(error_r)) {
		delete bis;
		return NULL;
	}

	return &bis->base;
}

static void
bz2_is_close(struct input_stream *is)
{
	Bzip2InputStream *bis = (Bzip2InputStream *)is;

	bis->Close();
	delete bis;
}

static bool
bz2_fillbuffer(Bzip2InputStream *bis, GError **error_r)
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
	Bzip2InputStream *bis = (Bzip2InputStream *)is;
	bz_stream *bzstream;
	int bz_result;
	size_t nbytes = 0;

	if (bis->eof)
		return 0;

	bzstream = &bis->bzstream;
	bzstream->next_out = (char *)ptr;
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
	Bzip2InputStream *bis = (Bzip2InputStream *)is;

	return bis->eof;
}

/* exported structures */

static const char *const bz2_extensions[] = {
	"bz2",
	NULL
};

const struct input_plugin bz2_inputplugin = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	bz2_is_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	bz2_is_read,
	bz2_is_eof,
	nullptr,
};

const struct archive_plugin bz2_archive_plugin = {
	"bz2",
	nullptr,
	nullptr,
	bz2_open,
	bz2_scan_reset,
	bz2_scan_next,
	bz2_open_stream,
	bz2_close,
	bz2_extensions,
};

