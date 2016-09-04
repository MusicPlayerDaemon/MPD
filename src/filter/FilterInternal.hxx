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

/** \file
 *
 * Internal stuff for the filter core and filter plugins.
 */

#ifndef MPD_FILTER_INTERNAL_HXX
#define MPD_FILTER_INTERNAL_HXX

#include "AudioFormat.hxx"

#include <stddef.h>

struct AudioFormat;
class Error;
template<typename T> struct ConstBuffer;

class Filter {
protected:
	AudioFormat out_audio_format;

	Filter() = default;
	explicit Filter(AudioFormat _out_audio_format)
		:out_audio_format(_out_audio_format) {}

public:
	virtual ~Filter() {}

	/**
	 * Returns the #AudioFormat produced by FilterPCM().
	 */
	const AudioFormat &GetOutAudioFormat() const {
		return out_audio_format;
	}

	/**
	 * Filters a block of PCM data.
	 *
	 * @param src the input buffer
	 * @param error location to store the error occurring
	 * @return the destination buffer on success (will be
	 * invalidated by deleting this object or the next FilterPCM()
	 * call), nullptr on error
	 */
	virtual ConstBuffer<void> FilterPCM(ConstBuffer<void> src, Error &error) = 0;
};

class PreparedFilter {
public:
	virtual ~PreparedFilter() {}

	/**
	 * Opens the filter, preparing it for FilterPCM().
	 *
	 * @param af the audio format of incoming data; the
	 * plugin may modify the object to enforce another input
	 * format
	 * @param error location to store the error occurring
	 */
	virtual Filter *Open(AudioFormat &af, Error &error) = 0;
};

#endif
