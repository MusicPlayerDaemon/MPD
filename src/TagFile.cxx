/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "TagFile.hxx"
#include "tag/Generic.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "fs/Path.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"

#include <cassert>

class TagFileScan {
	const Path path_fs;
	const char *const suffix;

	TagHandler &handler;

	Mutex mutex;
	InputStreamPtr is;

public:
	TagFileScan(Path _path_fs, const char *_suffix,
		    TagHandler &_handler) noexcept
		:path_fs(_path_fs), suffix(_suffix),
		 handler(_handler),
		 is(nullptr) {}

	bool ScanFile(const DecoderPlugin &plugin) noexcept {
		return plugin.ScanFile(path_fs, handler);
	}

	bool ScanStream(const DecoderPlugin &plugin) {
		if (plugin.scan_stream == nullptr)
			return false;

		/* open the InputStream (if not already open) */
		if (is == nullptr) {
			is = OpenLocalInputStream(path_fs, mutex);
		} else {
			is->LockRewind();
		}

		/* now try the stream_tag() method */
		return plugin.ScanStream(*is, handler);
	}

	bool Scan(const DecoderPlugin &plugin) {
		return plugin.SupportsSuffix(suffix) &&
			(ScanFile(plugin) || ScanStream(plugin));
	}
};

bool
ScanFileTagsNoGeneric(Path path_fs, TagHandler &handler)
{
	assert(!path_fs.IsNull());

	/* check if there's a suffix and a plugin */

	const auto *suffix = path_fs.GetSuffix();
	if (suffix == nullptr)
		return false;

	const auto suffix_utf8 = Path::FromFS(suffix).ToUTF8();

	TagFileScan tfs(path_fs, suffix_utf8.c_str(), handler);
	return decoder_plugins_try([&](const DecoderPlugin &plugin){
			return tfs.Scan(plugin);
		});
}

bool
ScanFileTagsWithGeneric(Path path, TagBuilder &builder,
			AudioFormat *audio_format)
{
	FullTagHandler h(builder, audio_format);

	if (!ScanFileTagsNoGeneric(path, h))
		return false;

	if (builder.empty())
		ScanGenericTags(path, h);

	return true;
}
