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
	DecoderCommand SubmitData(InputStream *is,
				  const void *data, size_t length,
				  uint16_t kbit_rate) noexcept override;

	DecoderCommand SubmitTag(InputStream *, Tag &&) noexcept override {
		return GetCommand();
	}

	void SubmitReplayGain(const ReplayGainInfo *) noexcept override {}
	void SubmitMixRamp(MixRampInfo &&) noexcept override {}
};

#endif
