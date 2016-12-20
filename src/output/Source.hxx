/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#ifndef AUDIO_OUTPUT_SOURCE_HXX
#define AUDIO_OUTPUT_SOURCE_HXX

#include "check.h"
#include "Compiler.h"
#include "SharedPipeConsumer.hxx"
#include "AudioFormat.hxx"
#include "ReplayGainMode.hxx"
#include "pcm/PcmBuffer.hxx"
#include "pcm/PcmDither.hxx"

#include <assert.h>

template<typename T> struct ConstBuffer;
struct MusicChunk;
class Filter;
class PreparedFilter;

/**
 * Source of audio data to be played by an #AudioOutput.  It receives
 * #MusicChunk instances from a #MusicPipe (via #SharedPipeConsumer).
 * It applies configured filters, ReplayGain and returns plain PCM
 * data.
 */
class AudioOutputSource {
	/**
	 * The audio_format in which audio data is received from the
	 * player thread (which in turn receives it from the decoder).
	 */
	AudioFormat in_audio_format = AudioFormat::Undefined();

	ReplayGainMode replay_gain_mode = ReplayGainMode::OFF;

	/**
	 * A reference to the #MusicPipe and the current position.
	 */
	SharedPipeConsumer pipe;

	/**
	 * The serial number of the last replay gain info.  0 means no
	 * replay gain info was available.
	 */
	unsigned replay_gain_serial = 0;

	/**
	 * The serial number of the last replay gain info by the
	 * "other" chunk during cross-fading.
	 */
	unsigned other_replay_gain_serial = 0;

	/**
	 * The replay_gain_filter_plugin instance of this audio
	 * output.
	 */
	Filter *replay_gain_filter_instance = nullptr;

	/**
	 * The replay_gain_filter_plugin instance of this audio
	 * output, to be applied to the second chunk during
	 * cross-fading.
	 */
	Filter *other_replay_gain_filter_instance = nullptr;

	/**
	 * The buffer used to allocate the cross-fading result.
	 */
	PcmBuffer cross_fade_buffer;

	/**
	 * The dithering state for cross-fading two streams.
	 */
	PcmDither cross_fade_dither;

	/**
	 * The filter object of this audio output.  This is an
	 * instance of chain_filter_plugin.
	 */
	Filter *filter_instance = nullptr;

public:
	void SetReplayGainMode(ReplayGainMode _mode) {
		replay_gain_mode = _mode;
	}

	bool IsOpen() const {
		return in_audio_format.IsDefined();
	}

	const AudioFormat &GetInputAudioFormat() const {
		return in_audio_format;
	}

	AudioFormat Open(AudioFormat audio_format, const MusicPipe &_pipe,
			 PreparedFilter *prepared_replay_gain_filter,
			 PreparedFilter *prepared_other_replay_gain_filter,
			 PreparedFilter *prepared_filter);

	void Close();

	void Cancel() {
		pipe.Cancel();
	}

	const MusicChunk *Get() {
		assert(IsOpen());

		return pipe.Get();
	}

	void Consume(const MusicChunk &chunk) {
		assert(IsOpen());

		pipe.Consume(chunk);
	}

	bool IsChunkConsumed(const MusicChunk &chunk) const {
		assert(IsOpen());

		return pipe.IsConsumed(chunk);
	}

	void ClearTailChunk(const MusicChunk &chunk) {
		pipe.ClearTail(chunk);
	}

	ConstBuffer<void> FilterChunk(const MusicChunk &chunk);

private:
	void OpenFilter(AudioFormat audio_format,
			PreparedFilter *prepared_replay_gain_filter,
			PreparedFilter *prepared_other_replay_gain_filter,
			PreparedFilter *prepared_filter);

	void CloseFilter();

	ConstBuffer<void> GetChunkData(const MusicChunk &chunk,
				       Filter *replay_gain_filter,
				       unsigned *replay_gain_serial_p);
};

#endif
