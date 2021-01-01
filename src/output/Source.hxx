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

#ifndef AUDIO_OUTPUT_SOURCE_HXX
#define AUDIO_OUTPUT_SOURCE_HXX

#include "SharedPipeConsumer.hxx"
#include "ReplayGainMode.hxx"
#include "pcm/AudioFormat.hxx"
#include "pcm/Buffer.hxx"
#include "pcm/Dither.hxx"
#include "thread/Mutex.hxx"
#include "util/ConstBuffer.hxx"

#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>

struct MusicChunk;
struct Tag;
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
	unsigned replay_gain_serial;

	/**
	 * The serial number of the last replay gain info by the
	 * "other" chunk during cross-fading.
	 */
	unsigned other_replay_gain_serial;

	/**
	 * The replay_gain_filter_plugin instance of this audio
	 * output.
	 */
	std::unique_ptr<Filter> replay_gain_filter;

	/**
	 * The replay_gain_filter_plugin instance of this audio
	 * output, to be applied to the second chunk during
	 * cross-fading.
	 */
	std::unique_ptr<Filter> other_replay_gain_filter;

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
	std::unique_ptr<Filter> filter;

	/**
	 * The #MusicChunk currently being processed (see
	 * #pending_tag, #pending_data).
	 */
	const MusicChunk *current_chunk = nullptr;

	/**
	 * The #Tag to be processed by the #AudioOutput.  It is owned
	 * by #current_chunk (MusicChunk::tag).
	 */
	const Tag *pending_tag;

	/**
	 * Filtered #MusicChunk PCM data to be processed by the
	 * #AudioOutput.
	 */
	ConstBuffer<uint8_t> pending_data;

public:
	AudioOutputSource() noexcept;
	~AudioOutputSource() noexcept;

	void SetReplayGainMode(ReplayGainMode _mode) noexcept {
		replay_gain_mode = _mode;
	}

	bool IsOpen() const {
		return in_audio_format.IsDefined();
	}

	const AudioFormat &GetInputAudioFormat() const {
		assert(IsOpen());

		return in_audio_format;
	}

	AudioFormat Open(AudioFormat audio_format, const MusicPipe &_pipe,
			 PreparedFilter *prepared_replay_gain_filter,
			 PreparedFilter *prepared_other_replay_gain_filter,
			 PreparedFilter &prepared_filter);

	void Close() noexcept;
	void Cancel() noexcept;

	/**
	 * Ensure that ReadTag() or PeekData() return any input.
	 *
	 * Throws on error
	 *
	 * @param mutex the #Mutex which protects the
	 * #SharedPipeConsumer; it is locked by the caller, and may be
	 * unlocked temporarily by this method
	 * @return true if any input is available, false if the source
	 * has (temporarily?) run empty
	 */
	bool Fill(Mutex &mutex);

	/**
	 * Reads the #Tag to be processed.  Be sure to call Fill()
	 * successfully before calling this metohd.
	 */
	const Tag *ReadTag() noexcept {
		assert(current_chunk != nullptr);

		return std::exchange(pending_tag, nullptr);
	}

	/**
	 * Returns the remaining filtered PCM data be played.  The
	 * caller shall use ConsumeData() to mark portions of the
	 * return value as "consumed".
	 *
	 * Be sure to call Fill() successfully before calling this
	 * metohd.
	 */
	ConstBuffer<void> PeekData() const noexcept {
		return pending_data.ToVoid();
	}

	/**
	 * Mark portions of the PeekData() return value as "consumed".
	 */
	void ConsumeData(size_t nbytes) noexcept;

	bool IsChunkConsumed(const MusicChunk &chunk) const  noexcept {
		assert(IsOpen());

		return pipe.IsConsumed(chunk);
	}

	void ClearTailChunk(const MusicChunk &chunk) noexcept {
		pipe.ClearTail(chunk);
	}

	/**
	 * Wrapper for Filter::Flush().
	 */
	ConstBuffer<void> Flush();

private:
	void OpenFilter(AudioFormat audio_format,
			PreparedFilter *prepared_replay_gain_filter,
			PreparedFilter *prepared_other_replay_gain_filter,
			PreparedFilter &prepared_filter);

	void CloseFilter() noexcept;

	ConstBuffer<void> GetChunkData(const MusicChunk &chunk,
				       Filter *replay_gain_filter,
				       unsigned *replay_gain_serial_p);

	ConstBuffer<void> FilterChunk(const MusicChunk &chunk);

	void DropCurrentChunk() noexcept {
		assert(current_chunk != nullptr);

		pipe.Consume(*std::exchange(current_chunk, nullptr));
	}
};

#endif
