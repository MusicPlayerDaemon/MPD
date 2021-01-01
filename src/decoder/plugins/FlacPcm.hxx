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

#ifndef MPD_FLAC_PCM_HXX
#define MPD_FLAC_PCM_HXX

#include "pcm/Buffer.hxx"
#include "pcm/AudioFormat.hxx"

#include <FLAC/ordinals.h>

template<typename T> struct ConstBuffer;

/**
 * This class imports libFLAC PCM data into a PCM format supported by
 * MPD.
 */
class FlacPcmImport {
	PcmBuffer buffer;

	AudioFormat audio_format;

public:
	/**
	 * Throws #std::runtime_error on error.
	 */
	void Open(unsigned sample_rate, unsigned bits_per_sample,
		  unsigned channels);

	const AudioFormat &GetAudioFormat() const {
		return audio_format;
	}

	ConstBuffer<void> Import(const FLAC__int32 *const src[],
				 size_t n_frames);
};

#endif
