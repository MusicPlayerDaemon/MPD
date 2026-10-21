// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Context.hxx"
#include "Chrono.hxx"
#include "decoder/Client.hxx"
#include "thread/Mutex.hxx"

#include <cstdint>
#include <exception>
#include <memory>

class PcmConvert;

class ChromaprintDecoderClient : public DecoderClient {
	bool ready = false;

	/**
	 * Is the initial seek to #start_time pending?  This is set by
	 * Ready() and cleared by CommandFinished() or SeekError().
	 */
	bool seeking = false;

	/**
	 * The start of the range to be fingerprinted (e.g. of a CUE
	 * track); zero means the beginning of the file.
	 */
	SongTime start_time = SongTime::zero();

	/**
	 * The end of the range to be fingerprinted; zero means the end
	 * of the file.
	 */
	SongTime end_time = SongTime::zero();

	unsigned sample_rate;

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

	/**
	 * Fingerprint only the specified range of the song.  Must be
	 * called before decoding starts.
	 */
	void SetRange(SongTime _start_time, SongTime _end_time) noexcept {
		start_time = _start_time;
		end_time = _end_time;
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
		if (IsStopRequested())
			return DecoderCommand::STOP;

		if (seeking)
			return DecoderCommand::SEEK;

		return DecoderCommand::NONE;
	}

	bool IsStopRequested() const noexcept override {
		return error || (ready && remaining_bytes == 0);
	}

	void CommandFinished() noexcept override {
		seeking = false;
	}

	SongTime GetSeekTime() noexcept override {
		return start_time;
	}

	uint64_t GetSeekFrame() noexcept override {
		return start_time.ToScale<uint64_t>(sample_rate);
	}

	void SeekError(std::exception_ptr &&_error) noexcept override;

	//InputStreamPtr OpenUri(std::string_view uri) override;

	bool Seek(InputStream &is, offset_type new_offset) noexcept override;
	size_t Read(InputStream &is,
		    std::span<std::byte> dest) noexcept override;

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
