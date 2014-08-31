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

#include "config.h"
#include "PcmExport.hxx"
#include "PcmDop.hxx"
#include "PcmPack.hxx"
#include "util/ByteReverse.hxx"
#include "util/ConstBuffer.hxx"

#include <iterator>

void
PcmExport::Open(SampleFormat sample_format, unsigned _channels,
		bool _dop, bool _shift8, bool _pack, bool _reverse_endian)
{
	assert(audio_valid_sample_format(sample_format));
	assert(!_dop || audio_valid_channel_count(_channels));

	channels = _channels;
	dop = _dop && sample_format == SampleFormat::DSD;
	if (dop)
		/* after the conversion to DoP, the DSD
		   samples are stuffed inside fake 24 bit samples */
		sample_format = SampleFormat::S24_P32;

	shift8 = _shift8 && sample_format == SampleFormat::S24_P32;
	pack24 = _pack && sample_format == SampleFormat::S24_P32;

	assert(!shift8 || !pack24);

	reverse_endian = 0;
	if (_reverse_endian) {
		size_t sample_size = pack24
			? 3
			: sample_format_size(sample_format);
		assert(sample_size <= 0xff);

		if (sample_size > 1)
			reverse_endian = sample_size;
	}
}

size_t
PcmExport::GetFrameSize(const AudioFormat &audio_format) const
{
	if (pack24)
		/* packed 24 bit samples (3 bytes per sample) */
		return audio_format.channels * 3;

	if (dop)
		/* the DSD-over-USB draft says that DSD 1-bit samples
		   are enclosed within 24 bit samples, and MPD's
		   representation of 24 bit is padded to 32 bit (4
		   bytes per sample) */
		return channels * 4;

	return audio_format.GetFrameSize();
}

ConstBuffer<void>
PcmExport::Export(ConstBuffer<void> data)
{
	if (dop)
		data = pcm_dsd_to_dop(dop_buffer, channels,
				      ConstBuffer<uint8_t>::FromVoid(data))
			.ToVoid();

	if (pack24) {
		const auto src = ConstBuffer<int32_t>::FromVoid(data);
		const size_t num_samples = src.size;
		const size_t dest_size = num_samples * 3;
		uint8_t *dest = (uint8_t *)pack_buffer.Get(dest_size);
		assert(dest != nullptr);

		pcm_pack_24(dest, src.begin(), src.end());

		data.data = dest;
		data.size = dest_size;
	} else if (shift8) {
		const auto src = ConstBuffer<int32_t>::FromVoid(data);

		uint32_t *dest = (uint32_t *)pack_buffer.Get(data.size);
		data.data = dest;

		for (auto i : src)
			*dest++ = i << 8;
	}

	if (reverse_endian > 0) {
		assert(reverse_endian >= 2);

		const auto src = ConstBuffer<uint8_t>::FromVoid(data);

		uint8_t *dest = (uint8_t *)reverse_buffer.Get(data.size);
		assert(dest != nullptr);
		data.data = dest;

		reverse_bytes(dest, src.begin(), src.end(), reverse_endian);
	}

	return data;
}

size_t
PcmExport::CalcSourceSize(size_t size) const
{
	if (pack24)
		/* 32 bit to 24 bit conversion (4 to 3 bytes) */
		size = (size / 3) * 4;

	if (dop)
		/* DoP doubles the transport size */
		size /= 2;

	return size;
}
