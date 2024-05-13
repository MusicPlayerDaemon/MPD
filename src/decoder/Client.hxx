// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Command.hxx"
#include "Chrono.hxx"
#include "input/Ptr.hxx"

#include <cstddef>
#include <cstdint>
#include <span>

struct AudioFormat;
struct Tag;
struct ReplayGainInfo;
class MixRampInfo;

/**
 * An interface between the decoder plugin and the MPD core.
 */
class DecoderClient {
public:
	/**
	 * Notify the client that it has finished initialization and
	 * that it has read the song's meta data.
	 *
	 * @param audio_format the audio format which is going to be
	 * sent to SubmitAudio()
	 * @param seekable true if the song is seekable
	 * @param duration the total duration of this song; negative if
	 * unknown
	 */
	virtual void Ready(AudioFormat audio_format,
			   bool seekable,
			   SignedSongTime duration) noexcept = 0;

	/**
	 * Determines the pending decoder command.
	 *
	 * @return the current command, or DecoderCommand::NONE if there is no
	 * command pending
	 */
	[[gnu::pure]]
	virtual DecoderCommand GetCommand() noexcept = 0;

	/**
	 * Called by the decoder when it has performed the requested command
	 * (dc->command).  This function resets dc->command and wakes up the
	 * player thread.
	 */
	virtual void CommandFinished() noexcept = 0;

	/**
	 * Call this when you have received the DecoderCommand::SEEK command.
	 *
	 * @return the destination position for the seek
	 */
	[[gnu::pure]]
	virtual SongTime GetSeekTime() noexcept = 0;

	/**
	 * Call this when you have received the DecoderCommand::SEEK command.
	 *
	 * @return the destination position for the seek in frames
	 */
	[[gnu::pure]]
	virtual uint64_t GetSeekFrame() noexcept = 0;

	/**
	 * Call this instead of CommandFinished() when seeking has
	 * failed.
	 */
	virtual void SeekError() noexcept = 0;

	/**
	 * Open a new #InputStream and wait until it's ready.
	 *
	 * Throws #StopDecoder if DecoderCommand::STOP was received.
	 *
	 * Throws std::runtime_error on error.
	 */
	virtual InputStreamPtr OpenUri(const char *uri) = 0;

	/**
	 * Blocking read from the input stream.
	 *
	 * @param is the input stream to read from
	 * @param buffer the destination buffer
	 * @param length the maximum number of bytes to read
	 * @return the number of bytes read, or 0 if one of the following
	 * occurs: end of file; error; command (like SEEK or STOP).
	 */
	virtual size_t Read(InputStream &is,
			    std::span<std::byte> dest) noexcept = 0;

	/**
	 * Sets the time stamp for the next data chunk [seconds].  The MPD
	 * core automatically counts it up, and a decoder plugin only needs to
	 * use this function if it thinks that adding to the time stamp based
	 * on the buffer size won't work.
	 */
	virtual void SubmitTimestamp(FloatDuration t) noexcept = 0;

	/**
	 * This function is called by the decoder plugin when it has
	 * successfully decoded block of input data.
	 *
	 * @param is an input stream which is buffering while we are waiting
	 * for the player
	 * @param data the source buffer
	 * @param length the number of bytes in the buffer
	 * @return the current command, or DecoderCommand::NONE if there is no
	 * command pending
	 */
	virtual DecoderCommand SubmitAudio(InputStream *is,
					   std::span<const std::byte> audio,
					   uint16_t kbit_rate) noexcept = 0;

	DecoderCommand SubmitAudio(InputStream &is,
				   std::span<const std::byte> audio,
				   uint16_t kbit_rate) noexcept {
		return SubmitAudio(&is, audio, kbit_rate);
	}

	template<typename T, std::size_t extent>
	DecoderCommand SubmitAudio(InputStream *is,
				   std::span<T, extent> audio,
				   uint16_t kbit_rate) noexcept {
		const std::span<const std::byte> audio_bytes =
			std::as_bytes(audio);
		return SubmitAudio(is, audio_bytes, kbit_rate);
	}

	template<typename T, std::size_t extent>
	DecoderCommand SubmitAudio(InputStream &is,
				   std::span<T, extent> audio,
				   uint16_t kbit_rate) noexcept {
		const std::span<const std::byte> audio_bytes =
			std::as_bytes(audio);
		return SubmitAudio(is, audio_bytes, kbit_rate);
	}

	/**
	 * This function is called by the decoder plugin when it has
	 * successfully decoded a tag.
	 *
	 * @param is an input stream which is buffering while we are waiting
	 * for the player
	 * @param tag the tag to send
	 * @return the current command, or DecoderCommand::NONE if there is no
	 * command pending
	 */
	virtual DecoderCommand SubmitTag(InputStream *is, Tag &&tag) noexcept = 0 ;

	DecoderCommand SubmitTag(InputStream &is, Tag &&tag) noexcept {
		return SubmitTag(&is, std::move(tag));
	}

	/**
	 * Set replay gain values for the following chunks.
	 *
	 * @param replay_gain_info the replay_gain_info object; may be nullptr
	 * to invalidate the previous replay gain values
	 */
	virtual void SubmitReplayGain(const ReplayGainInfo *replay_gain_info) noexcept = 0;

	/**
	 * Store MixRamp tags.
	 */
	virtual void SubmitMixRamp(MixRampInfo &&mix_ramp) noexcept = 0;
};
