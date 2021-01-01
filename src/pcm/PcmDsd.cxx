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

#include "PcmDsd.hxx"
#include "Dsd2Pcm.hxx"
#include "util/ConstBuffer.hxx"

#include <cassert>

ConstBuffer<float>
PcmDsd::ToFloat(unsigned channels, ConstBuffer<uint8_t> src) noexcept
{
	assert(!src.IsNull());
	assert(!src.empty());
	assert(src.size % channels == 0);

	const size_t num_samples = src.size;
	const size_t num_frames = src.size / channels;

	auto *dest = buffer.GetT<float>(num_samples);

	dsd2pcm.Translate(channels, num_frames, src.data, dest);
	return { dest, num_samples };
}

ConstBuffer<int32_t>
PcmDsd::ToS24(unsigned channels, ConstBuffer<uint8_t> src) noexcept
{
	assert(!src.IsNull());
	assert(!src.empty());
	assert(src.size % channels == 0);

	const size_t num_samples = src.size;
	const size_t num_frames = src.size / channels;

	auto *dest = buffer.GetT<int32_t>(num_samples);

	dsd2pcm.TranslateS24(channels, num_frames, src.data, dest);
	return { dest, num_samples };
}
