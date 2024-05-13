// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef DUMP_DECODER_CLIENT_HXX
#define DUMP_DECODER_CLIENT_HXX

#include "decoder/Client.hxx"
#include "thread/Mutex.hxx"

/**
 * A #DecoderClient implementation which dumps metadata to stderr and
 * decoded data to stdout.
 */
class DumpDecoderClient : public DecoderClient {
	bool initialized = false;

	uint16_t prev_kbit_rate = 0;

public:
	Mutex mutex;

	bool IsInitialized() const noexcept {
		return initialized;
	}

	/* virtual methods from DecoderClient */
	void Ready(AudioFormat audio_format,
		   bool seekable, SignedSongTime duration) noexcept override;
	DecoderCommand GetCommand() noexcept override;
	void CommandFinished() noexcept override;
	SongTime GetSeekTime() noexcept override;
	uint64_t GetSeekFrame() noexcept override;
	void SeekError() noexcept override;
	InputStreamPtr OpenUri(const char *uri) override;
	size_t Read(InputStream &is,
		    std::span<std::byte> dest) noexcept override;
	void SubmitTimestamp(FloatDuration t) noexcept override;
	DecoderCommand SubmitAudio(InputStream *is,
				   std::span<const std::byte> audio,
				   uint16_t kbit_rate) noexcept override;
	DecoderCommand SubmitTag(InputStream *is, Tag &&tag) noexcept override;
	void SubmitReplayGain(const ReplayGainInfo *replay_gain_info) noexcept override;
	void SubmitMixRamp(MixRampInfo &&mix_ramp) noexcept override;
};

#endif
