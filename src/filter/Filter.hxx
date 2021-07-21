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

#ifndef MPD_FILTER_HXX
#define MPD_FILTER_HXX

#include "pcm/AudioFormat.hxx"

#include <cassert>

template<typename T> struct ConstBuffer;

class Filter {
protected:
	AudioFormat out_audio_format;

	explicit Filter(AudioFormat _out_audio_format) noexcept
		:out_audio_format(_out_audio_format) {
		assert(out_audio_format.IsValid());
	}

public:
	virtual ~Filter() noexcept = default;

	/**
	 * Returns the #AudioFormat produced by FilterPCM().
	 */
	const AudioFormat &GetOutAudioFormat() const noexcept {
		return out_audio_format;
	}

	/**
	 * Reset the filter's state, e.g. drop/flush buffers.
	 */
	virtual void Reset() noexcept {
	}

	/**
	 * Filters a block of PCM data.
	 *
	 * Throws on error.
	 *
	 * @param src the input buffer
	 * @return the destination buffer on success (will be
	 * invalidated by deleting this object or the next FilterPCM()
	 * or Reset() call)
	 */
	virtual ConstBuffer<void> FilterPCM(ConstBuffer<void> src) = 0;

	/**
	 * Flush pending data and return it.  This should be called
	 * repeatedly until it returns nullptr.
	 *
	 * Throws on error.
	 */
	virtual ConstBuffer<void> Flush();
};

#endif
