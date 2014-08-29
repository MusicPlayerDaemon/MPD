/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_MUSIC_CHUNK_HXX
#define MPD_MUSIC_CHUNK_HXX

#include "Chrono.hxx"
#include "ReplayGainInfo.hxx"
#include "util/WritableBuffer.hxx"

#ifndef NDEBUG
#include "AudioFormat.hxx"
#endif

#include <stdint.h>
#include <stddef.h>

static constexpr size_t CHUNK_SIZE = 4096;

struct AudioFormat;
struct Tag;

/**
 * A chunk of music data.  Its format is defined by the
 * MusicPipe::Push() caller.
 */
struct MusicChunk {
	/** the next chunk in a linked list */
	MusicChunk *next;

	/**
	 * An optional chunk which should be mixed into this chunk.
	 * This is used for cross-fading.
	 */
	MusicChunk *other;

	/**
	 * The current mix ratio for cross-fading: 1.0 means play 100%
	 * of this chunk, 0.0 means play 100% of the "other" chunk.
	 */
	float mix_ratio;

	/** number of bytes stored in this chunk */
	uint16_t length;

	/** current bit rate of the source file */
	uint16_t bit_rate;

	/** the time stamp within the song */
	SignedSongTime time;

	/**
	 * An optional tag associated with this chunk (and the
	 * following chunks); appears at song boundaries.  The tag
	 * object is owned by this chunk, and must be freed when this
	 * chunk is deinitialized.
	 */
	Tag *tag;

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

	/** the data (probably PCM) */
	uint8_t data[CHUNK_SIZE];

#ifndef NDEBUG
	AudioFormat audio_format;
#endif

	MusicChunk()
		:other(nullptr),
		 length(0),
		 tag(nullptr),
		 replay_gain_serial(0) {}

	~MusicChunk();

	bool IsEmpty() const {
		return length == 0 && tag == nullptr;
	}

#ifndef NDEBUG
	/**
	 * Checks if the audio format if the chunk is equal to the
	 * specified audio_format.
	 */
	gcc_pure
	bool CheckFormat(AudioFormat audio_format) const;
#endif

	/**
	 * Prepares appending to the music chunk.  Returns a buffer
	 * where you may write into.  After you are finished, call
	 * Expand().
	 *
	 * @param chunk the MusicChunk object
	 * @param audio_format the audio format for the appended data;
	 * must stay the same for the life cycle of this chunk
	 * @param data_time the time within the song
	 * @param bit_rate the current bit rate of the source file
	 * @param max_length_r the maximum write length is returned
	 * here
	 * @return a writable buffer, or nullptr if the chunk is full
	 */
	WritableBuffer<void> Write(AudioFormat af,
				   SongTime data_time,
				   uint16_t bit_rate);

	/**
	 * Increases the length of the chunk after the caller has written to
	 * the buffer returned by Write().
	 *
	 * @param chunk the MusicChunk object
	 * @param audio_format the audio format for the appended data; must
	 * stay the same for the life cycle of this chunk
	 * @param length the number of bytes which were appended
	 * @return true if the chunk is full
	 */
	bool Expand(AudioFormat af, size_t length);
};

#endif
