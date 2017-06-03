/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#ifndef FAKE_DECODER_API_HXX
#define FAKE_DECODER_API_HXX

#include "check.h"
#include "decoder/Client.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"

struct FakeDecoder final : DecoderClient {
	Mutex mutex;
	Cond cond;

	bool initialized = false;

	/* virtual methods from DecoderClient */
	void Ready(AudioFormat audio_format,
		   bool seekable, SignedSongTime duration) override;
	DecoderCommand GetCommand() noexcept override;
	void CommandFinished() override;
	SongTime GetSeekTime() noexcept override;
	uint64_t GetSeekFrame() noexcept override;
	void SeekError() override;
	InputStreamPtr OpenUri(const char *uri) override;
	size_t Read(InputStream &is, void *buffer, size_t length) override;
	void SubmitTimestamp(double t) override;
	DecoderCommand SubmitData(InputStream *is,
				  const void *data, size_t length,
				  uint16_t kbit_rate) override;
	DecoderCommand SubmitTag(InputStream *is, Tag &&tag) override ;
	void SubmitReplayGain(const ReplayGainInfo *replay_gain_info) override;
	void SubmitMixRamp(MixRampInfo &&mix_ramp) override;
};

#endif
