// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FlacInput.hxx"
#include "FlacDomain.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "Log.hxx"

#include <exception>

inline FLAC__StreamDecoderReadStatus
FlacInput::Read(FLAC__byte buffer[], size_t *bytes) noexcept
{
	size_t r = decoder_read(client, input_stream, (void *)buffer, *bytes);
	*bytes = r;

	if (r == 0) {
		if (input_stream.LockIsEOF() ||
		    (client != nullptr &&
		     client->GetCommand() != DecoderCommand::NONE))
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		else
			return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}

	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

inline FLAC__StreamDecoderSeekStatus
FlacInput::Seek(FLAC__uint64 absolute_byte_offset) noexcept
{
	if (!input_stream.IsSeekable())
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;

	try {
		input_stream.LockSeek(absolute_byte_offset);
		return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
	} catch (...) {
		LogError(std::current_exception());
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	}
}

inline FLAC__StreamDecoderTellStatus
FlacInput::Tell(FLAC__uint64 *absolute_byte_offset) noexcept
{
	if (!input_stream.IsSeekable())
		return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;

	*absolute_byte_offset = (FLAC__uint64)input_stream.GetOffset();
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

inline FLAC__StreamDecoderLengthStatus
FlacInput::Length(FLAC__uint64 *stream_length) noexcept
{
	if (!input_stream.KnownSize())
		return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;

	*stream_length = (FLAC__uint64)input_stream.GetSize();
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

inline FLAC__bool
FlacInput::Eof() noexcept
{
	return (client != nullptr &&
		client->GetCommand() != DecoderCommand::NONE &&
		client->GetCommand() != DecoderCommand::SEEK) ||
		input_stream.LockIsEOF();
}

inline void
FlacInput::Error(FLAC__StreamDecoderErrorStatus status) noexcept
{
	if (client == nullptr ||
	    client->GetCommand() != DecoderCommand::STOP)
		LogWarning(flac_domain,
			   FLAC__StreamDecoderErrorStatusString[status]);
}

FLAC__StreamDecoderReadStatus
FlacInput::Read([[maybe_unused]] const FLAC__StreamDecoder *flac_decoder,
		FLAC__byte buffer[], size_t *bytes,
		void *client_data) noexcept
{
	auto *i = (FlacInput *)client_data;

	return i->Read(buffer, bytes);
}

FLAC__StreamDecoderSeekStatus
FlacInput::Seek([[maybe_unused]] const FLAC__StreamDecoder *flac_decoder,
		FLAC__uint64 absolute_byte_offset, void *client_data) noexcept
{
	auto *i = (FlacInput *)client_data;

	return i->Seek(absolute_byte_offset);
}

FLAC__StreamDecoderTellStatus
FlacInput::Tell([[maybe_unused]] const FLAC__StreamDecoder *flac_decoder,
		FLAC__uint64 *absolute_byte_offset, void *client_data) noexcept
{
	auto *i = (FlacInput *)client_data;

	return i->Tell(absolute_byte_offset);
}

FLAC__StreamDecoderLengthStatus
FlacInput::Length([[maybe_unused]] const FLAC__StreamDecoder *flac_decoder,
		  FLAC__uint64 *stream_length, void *client_data) noexcept
{
	auto *i = (FlacInput *)client_data;

	return i->Length(stream_length);
}

FLAC__bool
FlacInput::Eof([[maybe_unused]] const FLAC__StreamDecoder *flac_decoder,
	       void *client_data) noexcept
{
	auto *i = (FlacInput *)client_data;

	return i->Eof();
}

void
FlacInput::Error([[maybe_unused]] const FLAC__StreamDecoder *decoder,
		 FLAC__StreamDecoderErrorStatus status,
		 void *client_data) noexcept
{
	auto *i = (FlacInput *)client_data;

	i->Error(status);
}

