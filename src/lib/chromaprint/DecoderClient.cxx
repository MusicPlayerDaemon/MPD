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

	if (audio_format.format != SampleFormat::S16) {
		const AudioFormat src_audio_format = audio_format;
		audio_format.format = SampleFormat::S16;

		convert = std::make_unique<PcmConvert>(src_audio_format,
						       audio_format);
	}

	chromaprint.Start(audio_format.sample_rate, audio_format.channels);

	ready = true;
}

DecoderCommand
ChromaprintDecoderClient::SubmitAudio(InputStream *,
				      std::span<const std::byte> audio,
				      uint16_t) noexcept
{
	assert(ready);

	if (audio.size() > remaining_bytes)
		remaining_bytes = 0;
	else
		remaining_bytes -= audio.size();

	if (convert)
		audio = convert->Convert(audio);

	chromaprint.Feed(FromBytesStrict<const int16_t>(audio));

	return GetCommand();
}

size_t
ChromaprintDecoderClient::Read(InputStream &is,
			       void *buffer, size_t length) noexcept
{
	try {
		return is.LockRead(buffer, length);
	} catch (...) {
		error = std::current_exception();
		return 0;
	}
}
