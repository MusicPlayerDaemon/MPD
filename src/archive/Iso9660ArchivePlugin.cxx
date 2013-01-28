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
  * iso archive handling (requires cdio, and iso9660)
  */

#include "config.h"
#include "Iso9660ArchivePlugin.hxx"
#include "ArchiveInternal.hxx"
#include "ArchivePlugin.hxx"
#include "InputInternal.hxx"
#include "InputStream.hxx"
#include "InputPlugin.hxx"
#include "refcount.h"

#include <cdio/cdio.h>
#include <cdio/iso9660.h>

#include <glib.h>

#include <stdlib.h>
#include <string.h>

#define CEILING(x, y) ((x+(y-1))/y)

struct Iso9660ArchiveFile {
	struct archive_file base;

	struct refcount ref;

	iso9660_t *iso;
	GSList	*list;
	GSList	*iter;

	Iso9660ArchiveFile(iso9660_t *_iso)
		:iso(_iso), list(nullptr) {
		archive_file_init(&base, &iso9660_archive_plugin);
		refcount_init(&ref);
	}

	~Iso9660ArchiveFile() {
		//free list
		for (GSList *tmp = list; tmp != NULL; tmp = g_slist_next(tmp))
			g_free(tmp->data);
		g_slist_free(list);

		//close archive
		iso9660_close(iso);
	}

	void Unref() {
		if (refcount_dec(&ref))
			delete this;
	}

	void CollectRecursive(const char *path);
};

extern const struct input_plugin iso9660_input_plugin;

static inline GQuark
iso9660_quark(void)
{
	return g_quark_from_static_string("iso9660");
}

/* archive open && listing routine */

void
Iso9660ArchiveFile::CollectRecursive(const char *psz_path)
{
	CdioList_t *entlist;
	CdioListNode_t *entnode;
	iso9660_stat_t *statbuf;
	char pathname[4096];

	entlist = iso9660_ifs_readdir (iso, psz_path);
	if (!entlist) {
		return;
	}
	/* Iterate over the list of nodes that iso9660_ifs_readdir gives  */
	_CDIO_LIST_FOREACH (entnode, entlist) {
		statbuf = (iso9660_stat_t *) _cdio_list_node_data (entnode);

		strcpy(pathname, psz_path);
		strcat(pathname, statbuf->filename);

		if (iso9660_stat_s::_STAT_DIR == statbuf->type ) {
			if (strcmp(statbuf->filename, ".") && strcmp(statbuf->filename, "..")) {
				strcat(pathname, "/");
				CollectRecursive(pathname);
			}
		} else {
			//remove leading /
			list = g_slist_prepend(list, g_strdup(pathname + 1));
		}
	}
	_cdio_list_free (entlist, true);
}

static struct archive_file *
iso9660_archive_open(const char *pathname, GError **error_r)
{
	/* open archive */
	auto iso = iso9660_open(pathname);
	if (iso == nullptr) {
		g_set_error(error_r, iso9660_quark(), 0,
			    "Failed to open ISO9660 file %s", pathname);
		return NULL;
	}

	Iso9660ArchiveFile *archive = new Iso9660ArchiveFile(iso);
	archive->CollectRecursive("/");
	return &archive->base;
}

static void
iso9660_archive_scan_reset(struct archive_file *file)
{
	Iso9660ArchiveFile *context =
		(Iso9660ArchiveFile *)file;

	//reset iterator
	context->iter = context->list;
}

static char *
iso9660_archive_scan_next(struct archive_file *file)
{
	Iso9660ArchiveFile *context =
		(Iso9660ArchiveFile *)file;

	char *data = NULL;
	if (context->iter != NULL) {
		///fetch data and goto next
		data = (char *)context->iter->data;
		context->iter = g_slist_next(context->iter);
	}
	return data;
}

static void
iso9660_archive_close(struct archive_file *file)
{
	Iso9660ArchiveFile *context =
		(Iso9660ArchiveFile *)file;

	context->Unref();
}

/* single archive handling */

struct Iso9660InputStream {
	struct input_stream base;

	Iso9660ArchiveFile *archive;

	iso9660_stat_t *statbuf;
	size_t max_blocks;

	Iso9660InputStream(Iso9660ArchiveFile &_archive, const char *uri,
			   Mutex &mutex, Cond &cond,
			   iso9660_stat_t *_statbuf)
		:base(iso9660_input_plugin, uri, mutex, cond),
		      archive(&_archive), statbuf(_statbuf),
		 max_blocks(CEILING(statbuf->size, ISO_BLOCKSIZE)) {

		base.ready = true;
		base.size = statbuf->size;

		refcount_inc(&archive->ref);
	}

	~Iso9660InputStream() {
		free(statbuf);
		archive->Unref();
	}
};

static struct input_stream *
iso9660_archive_open_stream(struct archive_file *file, const char *pathname,
			    Mutex &mutex, Cond &cond,
			    GError **error_r)
{
	Iso9660ArchiveFile *context =
		(Iso9660ArchiveFile *)file;

	auto statbuf = iso9660_ifs_stat_translate(context->iso, pathname);
	if (statbuf == nullptr) {
		g_set_error(error_r, iso9660_quark(), 0,
			    "not found in the ISO file: %s", pathname);
		return NULL;
	}

	Iso9660InputStream *iis =
		new Iso9660InputStream(*context, pathname, mutex, cond,
				       statbuf);
	return &iis->base;
}

static void
iso9660_input_close(struct input_stream *is)
{
	Iso9660InputStream *iis = (Iso9660InputStream *)is;

	delete iis;
}


static size_t
iso9660_input_read(struct input_stream *is, void *ptr, size_t size, GError **error_r)
{
	Iso9660InputStream *iis = (Iso9660InputStream *)is;
	int toread, readed = 0;
	int no_blocks, cur_block;
	size_t left_bytes = iis->statbuf->size - is->offset;

	size = (size * ISO_BLOCKSIZE) / ISO_BLOCKSIZE;

	if (left_bytes < size) {
		toread = left_bytes;
		no_blocks = CEILING(left_bytes,ISO_BLOCKSIZE);
	} else {
		toread = size;
		no_blocks = toread / ISO_BLOCKSIZE;
	}
	if (no_blocks > 0) {

		cur_block = is->offset / ISO_BLOCKSIZE;

		readed = iso9660_iso_seek_read (iis->archive->iso, ptr,
			iis->statbuf->lsn + cur_block, no_blocks);

		if (readed != no_blocks * ISO_BLOCKSIZE) {
			g_set_error(error_r, iso9660_quark(), 0,
				    "error reading ISO file at lsn %lu",
				    (long unsigned int) cur_block);
			return 0;
		}
		if (left_bytes < size) {
			readed = left_bytes;
		}

		is->offset += readed;
	}
	return readed;
}

static bool
iso9660_input_eof(struct input_stream *is)
{
	return is->offset == is->size;
}

/* exported structures */

static const char *const iso9660_archive_extensions[] = {
	"iso",
	NULL
};

const struct input_plugin iso9660_input_plugin = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	iso9660_input_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	iso9660_input_read,
	iso9660_input_eof,
	nullptr,
};

const struct archive_plugin iso9660_archive_plugin = {
	"iso",
	nullptr,
	nullptr,
	iso9660_archive_open,
	iso9660_archive_scan_reset,
	iso9660_archive_scan_next,
	iso9660_archive_open_stream,
	iso9660_archive_close,
	iso9660_archive_extensions,
};
