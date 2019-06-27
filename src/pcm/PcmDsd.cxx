/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "PcmDsd.hxx"
#include "dsd2pcm/dsd2pcm.h"
#include "util/ConstBuffer.hxx"

#include <assert.h>

PcmDsd::PcmDsd() noexcept
{
	dsd2pcm.fill(nullptr);
}

PcmDsd::~PcmDsd() noexcept
{
	for (auto i : dsd2pcm)
		if (i != nullptr)
			dsd2pcm_destroy(i);
}

void
PcmDsd::Reset() noexcept
{
	for (auto i : dsd2pcm)
		if (i != nullptr)
			dsd2pcm_reset(i);
}

ConstBuffer<float>
PcmDsd::ToFloat(unsigned channels, ConstBuffer<uint8_t> src) noexcept
{
	assert(!src.IsNull());
	assert(!src.empty());
	assert(src.size % channels == 0);
	assert(channels <= dsd2pcm.max_size());

	const unsigned num_samples = src.size;
	const unsigned num_frames = src.size / channels;

	float *dest = buffer.GetT<float>(num_samples);

	for (unsigned c = 0; c < channels; ++c) {
		if (dsd2pcm[c] == nullptr) {
			dsd2pcm[c] = dsd2pcm_init();
			if (dsd2pcm[c] == nullptr)
				return nullptr;
		}

		dsd2pcm_translate(dsd2pcm[c], num_frames,
				  src.data + c, channels,
				  false, dest + c, channels);
	}

	return { dest, num_samples };
}
