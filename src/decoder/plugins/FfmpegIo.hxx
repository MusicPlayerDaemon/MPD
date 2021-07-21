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

#ifndef MPD_FFMPEG_IO_HXX
#define MPD_FFMPEG_IO_HXX

extern "C" {
#include "libavformat/avio.h"
}

#include <cstdint>

class DecoderClient;
class InputStream;

struct AvioStream {
	DecoderClient *const client;
	InputStream &input;

	AVIOContext *io;

	AvioStream(DecoderClient *_client, InputStream &_input)
		:client(_client), input(_input), io(nullptr) {}

	~AvioStream();

	bool Open();

private:
	int Read(void *buffer, int size);
	int64_t Seek(int64_t pos, int whence);

	static int _Read(void *opaque, uint8_t *buf, int size);
	static int64_t _Seek(void *opaque, int64_t pos, int whence);
};

#endif
