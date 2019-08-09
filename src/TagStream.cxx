/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "TagStream.hxx"
#include "tag/Generic.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "util/MimeType.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "input/InputStream.hxx"
#include "thread/Mutex.hxx"
#include "util/UriExtract.hxx"

#include <assert.h>

/**
 * Does the #DecoderPlugin support either the suffix or the MIME type?
 */
gcc_pure
static bool
CheckDecoderPlugin(const DecoderPlugin &plugin,
		   const char *suffix, const char *mime) noexcept
{
	return (mime != nullptr && plugin.SupportsMimeType(mime)) ||
		(suffix != nullptr && plugin.SupportsSuffix(suffix));
}

bool
tag_stream_scan(InputStream &is, TagHandler &handler) noexcept
{
	assert(is.IsReady());

	UriSuffixBuffer suffix_buffer;
	const char *const suffix = uri_get_suffix(is.GetURI(), suffix_buffer);
	const char *mime = is.GetMimeType();

	if (suffix == nullptr && mime == nullptr)
		return false;

	std::string mime_base;
	if (mime != nullptr)
		mime = (mime_base = GetMimeTypeBase(mime)).c_str();

	return decoder_plugins_try([suffix, mime, &is,
				    &handler](const DecoderPlugin &plugin){
			try {
				is.LockRewind();
			} catch (...) {
			}

			return CheckDecoderPlugin(plugin, suffix, mime) &&
				plugin.ScanStream(is, handler);
		});
}

bool
tag_stream_scan(const char *uri, TagHandler &handler)
{
	Mutex mutex;

	auto is = InputStream::OpenReady(uri, mutex);
	return tag_stream_scan(*is, handler);
}

bool
tag_stream_scan(InputStream &is, TagBuilder &builder,
		AudioFormat *audio_format) noexcept
{
	assert(is.IsReady());

	FullTagHandler h(builder, audio_format);

	if (!tag_stream_scan(is, h))
		return false;

	if (builder.empty())
		ScanGenericTags(is, h);

	return true;
}

bool
tag_stream_scan(const char *uri, TagBuilder &builder,
		AudioFormat *audio_format)
{
	Mutex mutex;

	auto is = InputStream::OpenReady(uri, mutex);
	return tag_stream_scan(*is, builder, audio_format);
}
