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

#include "config.h"
#include "TagFile.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "DecoderList.hxx"
#include "DecoderPlugin.hxx"
#include "InputStream.hxx"
#include "thread/Cond.hxx"

#include <assert.h>

bool
tag_file_scan(const char *path_fs,
	      const struct tag_handler *handler, void *handler_ctx)
{
	assert(path_fs != nullptr);
	assert(handler != nullptr);

	/* check if there's a suffix and a plugin */

	const char *suffix = uri_get_suffix(path_fs);
	if (suffix == nullptr)
		return false;

	const struct DecoderPlugin *plugin =
		decoder_plugin_from_suffix(suffix, nullptr);
	if (plugin == nullptr)
		return false;

	struct input_stream *is = nullptr;
	Mutex mutex;
	Cond cond;

	do {
		/* load file tag */
		if (plugin->ScanFile(path_fs,
				     *handler, handler_ctx))
			break;

		/* fall back to stream tag */
		if (plugin->scan_stream != nullptr) {
			/* open the input_stream (if not already
			   open) */
			if (is == nullptr)
				is = input_stream::Open(path_fs, mutex, cond,
							IgnoreError());

			/* now try the stream_tag() method */
			if (is != nullptr) {
				if (plugin->ScanStream(*is,
						       *handler, handler_ctx))
					break;

				is->LockRewind(IgnoreError());
			}
		}

		plugin = decoder_plugin_from_suffix(suffix, plugin);
	} while (plugin != nullptr);

	if (is != nullptr)
		is->Close();

	return plugin != nullptr;
}
