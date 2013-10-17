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

#include "config.h" /* must be first for large file support */
#include "Song.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "Directory.hxx"
#include "Mapper.hxx"
#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"
#include "InputStream.hxx"
#include "DecoderPlugin.hxx"
#include "DecoderList.hxx"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/TagHandler.hxx"
#include "tag/TagId3.hxx"
#include "tag/ApeTag.hxx"
#include "thread/Cond.hxx"

#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

Song *
Song::LoadFile(const char *path_utf8, Directory *parent)
{
	Song *song;
	bool ret;

	assert((parent == NULL) == Path::IsAbsoluteUTF8(path_utf8));
	assert(!uri_has_scheme(path_utf8));
	assert(strchr(path_utf8, '\n') == NULL);

	song = NewFile(path_utf8, parent);

	//in archive ?
	if (parent != NULL && parent->device == DEVICE_INARCHIVE) {
		ret = song->UpdateFileInArchive();
	} else {
		ret = song->UpdateFile();
	}
	if (!ret) {
		song->Free();
		return NULL;
	}

	return song;
}

/**
 * Attempts to load APE or ID3 tags from the specified file.
 */
static bool
tag_scan_fallback(const char *path,
		  const struct tag_handler *handler, void *handler_ctx)
{
	return tag_ape_scan2(path, handler, handler_ctx) ||
		tag_id3_scan(path, handler, handler_ctx);
}

bool
Song::UpdateFile()
{
	const char *suffix;
	const struct decoder_plugin *plugin;
	struct stat st;
	struct input_stream *is = NULL;

	assert(IsFile());

	/* check if there's a suffix and a plugin */

	suffix = uri_get_suffix(uri);
	if (suffix == NULL)
		return false;

	plugin = decoder_plugin_from_suffix(suffix, NULL);
	if (plugin == NULL)
		return false;

	const Path path_fs = map_song_fs(this);
	if (path_fs.IsNull())
		return false;

	delete tag;
	tag = nullptr;

	if (!StatFile(path_fs, st) || !S_ISREG(st.st_mode)) {
		return false;
	}

	mtime = st.st_mtime;

	Mutex mutex;
	Cond cond;

	TagBuilder tag_builder;

	do {
		/* load file tag */
		if (decoder_plugin_scan_file(plugin, path_fs.c_str(),
					     &full_tag_handler, &tag_builder))
			break;

		tag_builder.Clear();

		/* fall back to stream tag */
		if (plugin->scan_stream != NULL) {
			/* open the input_stream (if not already
			   open) */
			if (is == NULL)
				is = input_stream::Open(path_fs.c_str(),
							mutex, cond,
							IgnoreError());

			/* now try the stream_tag() method */
			if (is != NULL) {
				if (decoder_plugin_scan_stream(plugin, is,
							       &full_tag_handler,
							       &tag_builder))
					break;

				tag_builder.Clear();

				is->LockSeek(0, SEEK_SET, IgnoreError());
			}
		}

		plugin = decoder_plugin_from_suffix(suffix, plugin);
	} while (plugin != NULL);

	if (is != NULL)
		is->Close();

	if (!tag_builder.IsDefined())
		return false;

	if (tag_builder.IsEmpty())
		tag_scan_fallback(path_fs.c_str(), &full_tag_handler,
				  &tag_builder);

	tag = tag_builder.Commit();
	return true;
}

bool
Song::UpdateFileInArchive()
{
	const char *suffix;
	const struct decoder_plugin *plugin;

	assert(IsFile());

	/* check if there's a suffix and a plugin */

	suffix = uri_get_suffix(uri);
	if (suffix == NULL)
		return false;

	plugin = decoder_plugin_from_suffix(suffix, NULL);
	if (plugin == NULL)
		return false;

	delete tag;

	//accept every file that has music suffix
	//because we don't support tag reading through
	//input streams
	tag = new Tag();

	return true;
}
