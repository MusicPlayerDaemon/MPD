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

#include "archive_api.h"
#include "archive_list.h"
#include "input_archive.h"
#include "input_stream.h"
#include "gcc.h"
#include "log.h"
#include "ls.h"

#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <glib.h>

typedef struct {
	const struct archive_plugin *aplugin;
	const struct input_plugin *iplugin;
	struct archive_file *file;
} archive_context;

/**
 * select correct archive plugin to handle the input stream
 * may allow stacking of archive plugins. for example for handling
 * tar.gz a gzip handler opens file (through inputfile stream)
 * then it opens a tar handler and sets gzip inputstream as
 * parent_stream so tar plugin fetches file data from gzip
 * plugin and gzip fetches file from disk
 */
static bool
input_archive_open(struct input_stream *is, const char *pathname)
{
	archive_context *arch_ctx;
	const struct archive_plugin *arplug;
	char *archive, *filename, *suffix, *pname;
	bool opened;

	if (pathname[0] != '/')
		return false;

	pname = g_strdup(pathname);
	// archive_lookup will modify pname when true is returned
	if (!archive_lookup(pname, &archive, &filename, &suffix)) {
		g_debug("not an archive, lookup %s failed\n", pname);
		g_free(pname);
		return false;
	}

	//check which archive plugin to use (by ext)
	arplug = archive_plugin_from_suffix(suffix);
	if (!arplug) {
		g_warning("can't handle archive %s\n",archive);
		g_free(pname);
		return false;
	}

	arch_ctx = (archive_context *) g_malloc(sizeof(archive_context));

	//setup archive plugin pointer
	arch_ctx->aplugin = arplug;
	//open archive file
	arch_ctx->file = arplug->open(archive);
	//setup fileops
	arplug->setup_stream(arch_ctx->file, is);
	//setup input plugin backup
	arch_ctx->iplugin = is->plugin;
	is->plugin = &input_plugin_archive;

	//internal handle
	is->plugin = &input_plugin_archive;
	is->data = arch_ctx;

	//open archive
	opened = arch_ctx->iplugin->open(is, filename);

	if (!opened) {
		g_warning("open inarchive file %s failed\n\n",filename);
	} else {
		is->ready = true;
	}
	g_free(pname);
	return opened;
}

static void
input_archive_close(struct input_stream *is)
{
	archive_context *arch_ctx = (archive_context *)is->data;
	//close archive infile ops
	arch_ctx->iplugin->close(is);
	//close archive
	arch_ctx->aplugin->close(arch_ctx->file);
	//free private data
	g_free(arch_ctx);
}

static bool
input_archive_seek(struct input_stream *is, off_t offset, int whence)
{
	archive_context *arch_ctx = (archive_context *)is->data;
	return arch_ctx->iplugin->seek(is, offset, whence);
}

static size_t
input_archive_read(struct input_stream *is, void *ptr, size_t size)
{
	archive_context *arch_ctx = (archive_context *)is->data;
	assert(ptr != NULL);
	assert(size > 0);
	return arch_ctx->iplugin->read(is, ptr, size);
}

static bool
input_archive_eof(struct input_stream *is)
{
	archive_context *arch_ctx = (archive_context *)is->data;
	return arch_ctx->iplugin->eof(is);
}

static int
input_archive_buffer(struct input_stream *is)
{
	archive_context *arch_ctx = (archive_context *)is->data;
	return arch_ctx->iplugin->buffer(is);
}

const struct input_plugin input_plugin_archive = {
	.open = input_archive_open,
	.close = input_archive_close,
	.buffer = input_archive_buffer,
	.read = input_archive_read,
	.eof = input_archive_eof,
	.seek = input_archive_seek,
};
