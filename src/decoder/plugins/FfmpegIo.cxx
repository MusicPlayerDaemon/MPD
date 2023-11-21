// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/* necessary because libavutil/common.h uses UINT64_C */
#define __STDC_CONSTANT_MACROS

#include "FfmpegIo.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"

extern "C" {
#include <libavutil/mem.h>
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
#include <libavutil/error.h>
#endif
}

AvioStream::~AvioStream()
{
	if (io != nullptr) {
		av_free(io->buffer);
		av_free(io);
	}
}

inline int
AvioStream::Read(void *dest, int size)
{
	const auto nbytes = decoder_read(client, input, dest, size);
	if (nbytes == 0)
		return AVERROR_EOF;

	return nbytes;
}

inline int64_t
AvioStream::Seek(int64_t pos, int whence)
{
	switch (whence) {
	case SEEK_SET:
		break;

	case SEEK_CUR:
		pos += input.GetOffset();
		break;

	case SEEK_END:
		if (!input.KnownSize())
			return -1;

		pos += input.GetSize();
		break;

	case AVSEEK_SIZE:
		if (!input.KnownSize())
			return -1;

		return input.GetSize();

	default:
		return -1;
	}

	try {
		input.LockSeek(pos);
		return input.GetOffset();
	} catch (...) {
		return -1;
	}
}

int
AvioStream::_Read(void *opaque, uint8_t *buf, int size)
{
	AvioStream &stream = *(AvioStream *)opaque;

	return stream.Read(buf, size);
}

int64_t
AvioStream::_Seek(void *opaque, int64_t pos, int whence)
{
	AvioStream &stream = *(AvioStream *)opaque;

	return stream.Seek(pos, whence);
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
				_Read, nullptr,
				input.IsSeekable() ? _Seek : nullptr);
	/* If avio_alloc_context() fails, who frees the buffer?  The
	   libavformat API documentation does not specify this, it
	   only says that AVIOContext.buffer must be freed in the end,
	   however no AVIOContext exists in that failure code path. */
	return io != nullptr;
}
