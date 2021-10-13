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

#ifndef MPD_PCM_VOLUME_HXX
#define MPD_PCM_VOLUME_HXX

#include "SampleFormat.hxx"
#include "Buffer.hxx"
#include "Dither.hxx"

#ifndef NDEBUG
#include <cassert>
#endif

template<typename T> struct ConstBuffer;

/**
 * Number of fractional bits for a fixed-point volume value.
 */
static constexpr unsigned PCM_VOLUME_BITS = 10;

/**
 * This value means "100% volume".
 */
static constexpr unsigned PCM_VOLUME_1 = 1u << PCM_VOLUME_BITS;
static constexpr int PCM_VOLUME_1S = PCM_VOLUME_1;

/**
 * Converts a float value (0.0 = silence, 1.0 = 100% volume) to an
 * integer volume value (1000 = 100%).
 */
constexpr int
pcm_float_to_volume(float volume) noexcept
{
	return int(volume * PCM_VOLUME_1 + 0.5f);
}

constexpr float
pcm_volume_to_float(int volume) noexcept
{
	return (float)volume / (float)PCM_VOLUME_1;
}

/**
 * Software volume implementation.
 */
class PcmVolume {
	SampleFormat format;

	/**
	 * Are we currently converting to a different #SampleFormat?
	 * This is set by Open().
	 */
	bool convert;

	unsigned volume;

	PcmBuffer buffer;
	PcmDither dither;

public:
	PcmVolume() noexcept
		:volume(PCM_VOLUME_1) {
#ifndef NDEBUG
		format = SampleFormat::UNDEFINED;
#endif
	}

	unsigned GetVolume() const noexcept {
		return volume;
	}

	/**
	 * @param _volume the volume level in the range
	 * [0..#PCM_VOLUME_1]; may be bigger than #PCM_VOLUME_1, but
	 * then it will most likely clip a lot
	 */
	void SetVolume(unsigned _volume) noexcept {
		volume = _volume;
	}

	/**
	 * Opens the object, prepare for Apply().
	 *
	 * Throws on error.
	 *
	 * @param format the input sample format
	 * @param allow_convert allow the class to convert to a
	 * different #SampleFormat to preserve quality?
	 * @return the output sample format
	 */
	SampleFormat Open(SampleFormat format, bool allow_convert);

	/**
	 * Closes the object.  After that, you may call Open() again.
	 */
	void Close() noexcept {
#ifndef NDEBUG
		assert(format != SampleFormat::UNDEFINED);
		format = SampleFormat::UNDEFINED;
#endif
	}

	/**
	 * Apply the volume level.
	 */
	[[gnu::pure]]
	ConstBuffer<void> Apply(ConstBuffer<void> src) noexcept;
};

#endif
