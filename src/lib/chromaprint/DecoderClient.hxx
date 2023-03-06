// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef CHROMAPRINT_DECODER_CLIENT_HXX
#define CHROMAPRINT_DECODER_CLIENT_HXX

#include "Context.hxx"
#include "decoder/Client.hxx"
#include "thread/Mutex.hxx"

#include <cstdint>
#include <exception>
#include <memory>

class PcmConvert;

class ChromaprintDecoderClient : public DecoderClient {
	bool ready = false;

	std::unique_ptr<PcmConvert> convert;

	Chromaprint::Context chromaprint;

	uint64_t remaining_bytes;

protected:
	/**
	 * This is set when an I/O error occurs while decoding; it
	 * will be rethrown by Finish().
	 */
	std::exception_ptr error;

public:
	Mutex mutex;

	ChromaprintDecoderClient();
	~ChromaprintDecoderClient() noexcept;

	bool IsReady() const noexcept {
		return ready;
	}

	void Reset() noexcept {
	}

	void Finish();

	std::string GetFingerprint() const {
		return chromaprint.GetFingerprint();
	}

	/* virtual methods from DecoderClient */
	void Ready(AudioFormat audio_format,
		   bool seekable, SignedSongTime duration) noexcept override;

	DecoderCommand GetCommand() noexcept override {
		return !error && (!ready || remaining_bytes > 0)
			? DecoderCommand::NONE
			: DecoderCommand::STOP;
	}

	void CommandFinished() noexcept override {}

	SongTime GetSeekTime() noexcept override {
		return SongTime::zero();
	}

	uint64_t GetSeekFrame() noexcept override {
		return 0;
	}

	void SeekError() noexcept override {}

	//InputStreamPtr OpenUri(const char *) override;

	size_t Read(InputStream &is,
		    void *buffer, size_t length) noexcept override;

	void SubmitTimestamp(FloatDuration) noexcept override {}
	DecoderCommand SubmitAudio(InputStream *is,
				   std::span<const std::byte> audio,
				   uint16_t kbit_rate) noexcept override;

	DecoderCommand SubmitTag(InputStream *, Tag &&) noexcept override {
		return GetCommand();
	}

	void SubmitReplayGain(const ReplayGainInfo *) noexcept override {}
	void SubmitMixRamp(MixRampInfo &&) noexcept override {}
};

#endif
