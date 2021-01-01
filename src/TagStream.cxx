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

#include <cassert>

/**
 * Does the #DecoderPlugin support either the suffix or the MIME type?
 */
gcc_pure
static bool
CheckDecoderPlugin(const DecoderPlugin &plugin,
		   std::string_view suffix, std::string_view mime) noexcept
{
	return (!mime.empty() && plugin.SupportsMimeType(mime)) ||
		(!suffix.empty() && plugin.SupportsSuffix(suffix));
}

bool
tag_stream_scan(InputStream &is, TagHandler &handler)
{
	assert(is.IsReady());

	const auto suffix = uri_get_suffix(is.GetURI());
	const char *full_mime = is.GetMimeType();

	if (suffix.empty() && full_mime == nullptr)
		return false;

	std::string_view mime_base{};
	if (full_mime != nullptr)
		mime_base = GetMimeTypeBase(full_mime);

	return decoder_plugins_try([suffix, mime_base, &is,
				    &handler](const DecoderPlugin &plugin){
			try {
				is.LockRewind();
			} catch (...) {
			}

			return CheckDecoderPlugin(plugin, suffix, mime_base) &&
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
		AudioFormat *audio_format)
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
