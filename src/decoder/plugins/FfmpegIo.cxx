/*
 * Copyright (C) 2003-2016 The Music Player Daemon Project
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

/* necessary because libavutil/common.h uses UINT64_C */
#define __STDC_CONSTANT_MACROS

#include "config.h"
#include "FfmpegIo.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "util/Error.hxx"

AvioStream::~AvioStream()
{
	if (io != nullptr) {
		av_free(io->buffer);
		av_free(io);
	}
}

static int
mpd_ffmpeg_stream_read(void *opaque, uint8_t *buf, int size)
{
	AvioStream *stream = (AvioStream *)opaque;

	return decoder_read(stream->decoder, stream->input,
			    (void *)buf, size);
}

static int64_t
mpd_ffmpeg_stream_seek(void *opaque, int64_t pos, int whence)
{
	AvioStream *stream = (AvioStream *)opaque;

	switch (whence) {
	case SEEK_SET:
		break;

	case SEEK_CUR:
		pos += stream->input.GetOffset();
		break;

	case SEEK_END:
		if (!stream->input.KnownSize())
			return -1;

		pos += stream->input.GetSize();
		break;

	case AVSEEK_SIZE:
		if (!stream->input.KnownSize())
			return -1;

		return stream->input.GetSize();

	default:
		return -1;
	}

	if (!stream->input.LockSeek(pos, IgnoreError()))
		return -1;

	return stream->input.GetOffset();
}

bool
AvioStream::Open()
{
	constexpr size_t BUFFER_SIZE = 8192;
	auto buffer = (unsigned char *)av_malloc(BUFFER_SIZE);
	if (buffer == nullptr)
		return false;

	io = avio_alloc_context(buffer, BUFFER_SIZE,
				false, this,
				mpd_ffmpeg_stream_read, nullptr,
				input.IsSeekable()
				? mpd_ffmpeg_stream_seek : nullptr);
	/* If avio_alloc_context() fails, who frees the buffer?  The
	   libavformat API documentation does not specify this, it
	   only says that AVIOContext.buffer must be freed in the end,
	   however no AVIOContext exists in that failure code path. */
	return io != nullptr;
}
