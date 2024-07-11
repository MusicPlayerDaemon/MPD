// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

	const auto *suffix = path_fs.GetExtension();
	if (suffix == nullptr)
		return false;

	const auto suffix_utf8 = Path::FromFS(suffix).ToUTF8();

	TagFileScan tfs(path_fs, suffix_utf8.c_str(), handler);
	for (const auto &plugin : GetEnabledDecoderPlugins()) {
		if (tfs.Scan(plugin))
			return true;
	}

	return false;
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
