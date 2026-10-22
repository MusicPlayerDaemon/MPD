// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DecoderClient.hxx"
#include "pcm/Convert.hxx"
#include "input/InputStream.hxx"
#include "util/SpanCast.hxx"

ChromaprintDecoderClient::ChromaprintDecoderClient() = default;
ChromaprintDecoderClient::~ChromaprintDecoderClient() noexcept = default;

void
ChromaprintDecoderClient::Finish()
{
	if (error)
		std::rethrow_exception(error);

	if (!ready)
		throw std::runtime_error("Decoding failed");

	if (convert) {
		auto flushed = convert->Flush();
		chromaprint.Feed(FromBytesStrict<const int16_t>(flushed));
	}

	chromaprint.Finish();
}

void
ChromaprintDecoderClient::Ready(AudioFormat audio_format, bool,
				SignedSongTime) noexcept
{
	/* feed the first two minutes into libchromaprint */
	remaining_bytes = audio_format.TimeToSize(std::chrono::minutes(2));

	try {
		if (audio_format.format != SampleFormat::S16) {
			const AudioFormat src_audio_format = audio_format;
			audio_format.format = SampleFormat::S16;

			convert = std::make_unique<PcmConvert>(src_audio_format,
							       audio_format);
		}

		/* this fails if libchromaprint does not support the
		   audio format (e.g. a very low sample rate) */
		chromaprint.Start(audio_format.sample_rate, audio_format.channels);
	} catch (...) {
		/* GetCommand() returns STOP now, and Finish() will
		   rethrow this */
		error = std::current_exception();
		return;
	}

	ready = true;
}

DecoderCommand
ChromaprintDecoderClient::SubmitAudio(InputStream *,
				      std::span<const std::byte> audio,
				      uint16_t) noexcept
{
	if (error)
		/* Ready() or a previous call has failed */
		return DecoderCommand::STOP;

	assert(ready);

	if (audio.size() > remaining_bytes) {
		/* don't feed more than requested */
		audio = audio.first(remaining_bytes);
		remaining_bytes = 0;
	} else
		remaining_bytes -= audio.size();

	try {
		if (convert)
			audio = convert->Convert(audio);

		chromaprint.Feed(FromBytesStrict<const int16_t>(audio));
	} catch (...) {
		error = std::current_exception();
	}

	return GetCommand();
}

bool
ChromaprintDecoderClient::Seek(InputStream &is, offset_type new_offset) noexcept
try {
	is.LockSeek(new_offset);
	return true;
} catch (...) {
	error = std::current_exception();
	return false;
}

size_t
ChromaprintDecoderClient::Read(InputStream &is,
			       std::span<std::byte> dest) noexcept
{
	try {
		return is.LockRead(dest);
	} catch (...) {
		error = std::current_exception();
		return 0;
	}
}
