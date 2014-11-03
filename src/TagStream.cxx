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
#include "TagStream.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "input/InputStream.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"

#include <assert.h>

/**
 * Does the #DecoderPlugin support either the suffix or the MIME type?
 */
gcc_pure
static bool
CheckDecoderPlugin(const DecoderPlugin &plugin,
		   const char *suffix, const char *mime)
{
	return (mime != nullptr && plugin.SupportsMimeType(mime)) ||
		(suffix != nullptr && plugin.SupportsSuffix(suffix));
}

bool
tag_stream_scan(InputStream &is, const tag_handler &handler, void *ctx)
{
	assert(is.IsReady());

	UriSuffixBuffer suffix_buffer;
	const char *const suffix = uri_get_suffix(is.GetURI(), suffix_buffer);
	const char *const mime = is.GetMimeType();

	if (suffix == nullptr && mime == nullptr)
		return false;

	return decoder_plugins_try([suffix, mime, &is,
				    &handler, ctx](const DecoderPlugin &plugin){
			is.LockRewind(IgnoreError());

			return CheckDecoderPlugin(plugin, suffix, mime) &&
				plugin.ScanStream(is, handler, ctx);
		});
}

bool
tag_stream_scan(const char *uri, const tag_handler &handler, void *ctx)
{
	Mutex mutex;
	Cond cond;

	InputStream *is = InputStream::OpenReady(uri, mutex, cond,
						 IgnoreError());
	if (is == nullptr)
		return false;

	bool success = tag_stream_scan(*is, handler, ctx);
	delete is;
	return success;
}
