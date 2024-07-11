// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
[[gnu::pure]]
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

	for (const auto &plugin : GetEnabledDecoderPlugins()) {
		if (!CheckDecoderPlugin(plugin, suffix, mime_base))
			continue;

		try {
			is.LockRewind();
		} catch (...) {
		}

		if (plugin.ScanStream(is, handler))
			return true;
	}

	return false;
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
