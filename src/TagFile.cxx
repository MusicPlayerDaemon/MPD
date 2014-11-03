/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "fs/Path.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "thread/Cond.hxx"

#include <assert.h>

class TagFileScan {
	const Path path_fs;
	const char *const suffix;

	const tag_handler &handler;
	void *handler_ctx;

	Mutex mutex;
	Cond cond;
	InputStream *is;

public:
	TagFileScan(Path _path_fs, const char *_suffix,
		    const tag_handler &_handler, void *_handler_ctx)
		:path_fs(_path_fs), suffix(_suffix),
		 handler(_handler), handler_ctx(_handler_ctx) ,
		 is(nullptr) {}

	~TagFileScan() {
		delete is;
	}

	bool ScanFile(const DecoderPlugin &plugin) {
		return plugin.ScanFile(path_fs, handler, handler_ctx);
	}

	bool ScanStream(const DecoderPlugin &plugin) {
		if (plugin.scan_stream == nullptr)
			return false;

		/* open the InputStream (if not already open) */
		if (is == nullptr) {
			is = OpenLocalInputStream(path_fs,
						  mutex, cond,
						  IgnoreError());
			if (is == nullptr)
				return false;
		} else
			is->LockRewind(IgnoreError());

		/* now try the stream_tag() method */
		return plugin.ScanStream(*is, handler, handler_ctx);
	}

	bool Scan(const DecoderPlugin &plugin) {
		return plugin.SupportsSuffix(suffix) &&
			(ScanFile(plugin) || ScanStream(plugin));
	}
};

bool
tag_file_scan(Path path_fs, const tag_handler &handler, void *handler_ctx)
{
	assert(!path_fs.IsNull());

	/* check if there's a suffix and a plugin */

	const char *suffix = uri_get_suffix(path_fs.c_str());
	if (suffix == nullptr)
		return false;

	TagFileScan tfs(path_fs, suffix, handler, handler_ctx);
	return decoder_plugins_try([&](const DecoderPlugin &plugin){
			return tfs.Scan(plugin);
		});
}
