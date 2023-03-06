// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
	FLAC__StreamDecoderReadStatus Read(FLAC__byte buffer[], size_t *bytes) noexcept;
	FLAC__StreamDecoderSeekStatus Seek(FLAC__uint64 absolute_byte_offset) noexcept;
	FLAC__StreamDecoderTellStatus Tell(FLAC__uint64 *absolute_byte_offset) noexcept;
	FLAC__StreamDecoderLengthStatus Length(FLAC__uint64 *stream_length) noexcept;
	FLAC__bool Eof() noexcept;
	void Error(FLAC__StreamDecoderErrorStatus status) noexcept;

public:
	static FLAC__StreamDecoderReadStatus
	Read(const FLAC__StreamDecoder *flac_decoder,
	     FLAC__byte buffer[], size_t *bytes, void *client_data) noexcept;

	static FLAC__StreamDecoderSeekStatus
	Seek(const FLAC__StreamDecoder *flac_decoder,
	     FLAC__uint64 absolute_byte_offset, void *client_data) noexcept;

	static FLAC__StreamDecoderTellStatus
	Tell(const FLAC__StreamDecoder *flac_decoder,
	     FLAC__uint64 *absolute_byte_offset, void *client_data) noexcept;

	static FLAC__StreamDecoderLengthStatus
	Length(const FLAC__StreamDecoder *flac_decoder,
	       FLAC__uint64 *stream_length, void *client_data) noexcept;

	static FLAC__bool
	Eof(const FLAC__StreamDecoder *flac_decoder,
	    void *client_data) noexcept;

	static void
	Error(const FLAC__StreamDecoder *decoder,
	      FLAC__StreamDecoderErrorStatus status,
	      void *client_data) noexcept;
};

#endif
