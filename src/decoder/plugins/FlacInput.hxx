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

#ifndef MPD_FLAC_INPUT_HXX
#define MPD_FLAC_INPUT_HXX

#include <FLAC/stream_decoder.h>

class DecoderClient;
class InputStream;

/**
 * This class wraps an #InputStream in libFLAC stream decoder
 * callbacks.
 */
class FlacInput {
	DecoderClient *const client;

	InputStream &input_stream;

public:
	FlacInput(InputStream &_input_stream,
		  DecoderClient *_client=nullptr)
		:client(_client), input_stream(_input_stream) {}

	DecoderClient *GetClient() {
		return client;
	}

	InputStream &GetInputStream() {
		return input_stream;
	}

protected:
	FLAC__StreamDecoderReadStatus Read(FLAC__byte buffer[], size_t *bytes);
	FLAC__StreamDecoderSeekStatus Seek(FLAC__uint64 absolute_byte_offset);
	FLAC__StreamDecoderTellStatus Tell(FLAC__uint64 *absolute_byte_offset);
	FLAC__StreamDecoderLengthStatus Length(FLAC__uint64 *stream_length);
	FLAC__bool Eof();
	void Error(FLAC__StreamDecoderErrorStatus status);

public:
	static FLAC__StreamDecoderReadStatus
	Read(const FLAC__StreamDecoder *flac_decoder,
	     FLAC__byte buffer[], size_t *bytes, void *client_data);

	static FLAC__StreamDecoderSeekStatus
	Seek(const FLAC__StreamDecoder *flac_decoder,
	     FLAC__uint64 absolute_byte_offset, void *client_data);

	static FLAC__StreamDecoderTellStatus
	Tell(const FLAC__StreamDecoder *flac_decoder,
	     FLAC__uint64 *absolute_byte_offset, void *client_data);

	static FLAC__StreamDecoderLengthStatus
	Length(const FLAC__StreamDecoder *flac_decoder,
	       FLAC__uint64 *stream_length, void *client_data);

	static FLAC__bool
	Eof(const FLAC__StreamDecoder *flac_decoder, void *client_data);

	static void
	Error(const FLAC__StreamDecoder *decoder,
	      FLAC__StreamDecoderErrorStatus status, void *client_data);
};

#endif
