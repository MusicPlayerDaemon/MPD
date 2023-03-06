// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_MUSIC_CHUNK_HXX
#define MPD_MUSIC_CHUNK_HXX

#include "MusicChunkPtr.hxx"
#include "Chrono.hxx"
#include "tag/ReplayGainInfo.hxx"

#ifndef NDEBUG
#include "pcm/AudioFormat.hxx"
#endif

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

static constexpr size_t CHUNK_SIZE = 4096;

struct AudioFormat;
struct Tag;
struct MusicChunk;

/**
 * Meta information for #MusicChunk.
 */
struct MusicChunkInfo {
	/** the next chunk in a linked list */
	MusicChunkPtr next;

	/**
	 * An optional chunk which should be mixed into this chunk.
	 * This is used for cross-fading.
	 */
	MusicChunkPtr other;

	/**
	 * An optional tag associated with this chunk (and the
	 * following chunks); appears at song boundaries.
	 */
	std::unique_ptr<Tag> tag;

	/**
	 * The current mix ratio for cross-fading: 1.0 means play 100%
	 * of this chunk, 0.0 means play 100% of the "other" chunk.
	 */
	float mix_ratio;

	/** number of bytes stored in this chunk */
	uint16_t length = 0;

	/** current bit rate of the source file */
	uint16_t bit_rate;

	/** the time stamp within the song */
	SignedSongTime time;

	/**
	 * Replay gain information associated with this chunk.
	 * Only valid if the serial is not 0.
	 */
	ReplayGainInfo replay_gain_info;

	/**
	 * A serial number for checking if replay gain info has
	 * changed since the last chunk.  The magic value 0 indicates
	 * that there is no replay gain info available.
	 */
	unsigned replay_gain_serial;

#ifndef NDEBUG
	AudioFormat audio_format;
#endif

	MusicChunkInfo() noexcept;
	~MusicChunkInfo() noexcept;

	MusicChunkInfo(const MusicChunkInfo &) = delete;
	MusicChunkInfo &operator=(const MusicChunkInfo &) = delete;

	bool IsEmpty() const {
		return length == 0 && tag == nullptr;
	}

#ifndef NDEBUG
	/**
	 * Checks if the audio format if the chunk is equal to the
	 * specified audio_format.
	 */
	[[gnu::pure]]
	bool CheckFormat(AudioFormat audio_format) const noexcept;
#endif
};

/**
 * A chunk of music data.  Its format is defined by the
 * MusicPipe::Push() caller.
 */
struct MusicChunk : MusicChunkInfo {
	/** the data (probably PCM) */
	std::byte data[CHUNK_SIZE - sizeof(MusicChunkInfo)];

	/**
	 * Prepares appending to the music chunk.  Returns a buffer
	 * where you may write into.  After you are finished, call
	 * Expand().
	 *
	 * @param af the audio format for the appended data;
	 * must stay the same for the life cycle of this chunk
	 * @param data_time the time within the song
	 * @param bit_rate the current bit rate of the source file
	 * @return a writable buffer, or nullptr if the chunk is full
	 */
	std::span<std::byte> Write(AudioFormat af,
				   SongTime data_time,
				   uint16_t bit_rate) noexcept;

	/**
	 * Increases the length of the chunk after the caller has written to
	 * the buffer returned by Write().
	 *
	 * @param af the audio format for the appended data; must
	 * stay the same for the life cycle of this chunk
	 * @param length the number of bytes which were appended
	 * @return true if the chunk is full
	 */
	bool Expand(AudioFormat af, size_t length) noexcept;
};

static_assert(sizeof(MusicChunk) == CHUNK_SIZE, "Wrong size");

#endif
