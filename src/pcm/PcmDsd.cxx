/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "PcmDsd.hxx"
#include "dsd2pcm/dsd2pcm.h"

#include <glib.h>

#include <algorithm>

#include <assert.h>
#include <string.h>

PcmDsd::PcmDsd()
{
	std::fill_n(dsd2pcm, G_N_ELEMENTS(dsd2pcm), nullptr);
}

PcmDsd::~PcmDsd()
{
	for (unsigned i = 0; i < G_N_ELEMENTS(dsd2pcm); ++i)
		if (dsd2pcm[i] != nullptr)
			dsd2pcm_destroy(dsd2pcm[i]);
}

void
PcmDsd::Reset()
{
	for (unsigned i = 0; i < G_N_ELEMENTS(dsd2pcm); ++i)
		if (dsd2pcm[i] != nullptr)
			dsd2pcm_reset(dsd2pcm[i]);
}

const float *
PcmDsd::ToFloat(unsigned channels, bool lsbfirst,
		const uint8_t *src, size_t src_size,
		size_t *dest_size_r)
{
	assert(src != nullptr);
	assert(src_size > 0);
	assert(src_size % channels == 0);
	assert(channels <= G_N_ELEMENTS(dsd2pcm));

	const unsigned num_samples = src_size;
	const unsigned num_frames = src_size / channels;

	float *dest;
	const size_t dest_size = num_samples * sizeof(*dest);
	*dest_size_r = dest_size;
	dest = (float *)buffer.Get(dest_size);

	for (unsigned c = 0; c < channels; ++c) {
		if (dsd2pcm[c] == nullptr) {
			dsd2pcm[c] = dsd2pcm_init();
			if (dsd2pcm[c] == nullptr)
				return nullptr;
		}

		dsd2pcm_translate(dsd2pcm[c], num_frames,
				  src + c, channels,
				  lsbfirst, dest + c, channels);
	}

	return dest;
}
