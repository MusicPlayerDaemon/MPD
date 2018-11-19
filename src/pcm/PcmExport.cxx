/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "PcmExport.hxx"
#include "AudioFormat.hxx"
#include "Order.hxx"
#include "PcmPack.hxx"
#include "util/ByteReverse.hxx"
#include "util/ConstBuffer.hxx"

#ifdef ENABLE_DSD
#include "Dsd16.hxx"
#include "Dsd32.hxx"
#include "PcmDsd.hxx"
#include "PcmDop.hxx"
#endif

#include <assert.h>

void
PcmExport::Open(SampleFormat sample_format, unsigned _channels,
		Params params) noexcept
{
	assert(audio_valid_sample_format(sample_format));

	channels = _channels;
	alsa_channel_order = params.alsa_channel_order
		? sample_format
		: SampleFormat::UNDEFINED;

#ifdef ENABLE_DSD
	assert((params.dsd_u16 + params.dsd_u32 + params.dop) <= 1);
	assert(!params.dop || audio_valid_channel_count(_channels));

	dsd_u16 = params.dsd_u16 && sample_format == SampleFormat::DSD;
	if (dsd_u16)
		/* after the conversion to DSD_U16, the DSD samples
		   are stuffed inside fake 16 bit samples */
		sample_format = SampleFormat::S16;

	dsd_u32 = params.dsd_u32 && sample_format == SampleFormat::DSD;
	if (dsd_u32)
		/* after the conversion to DSD_U32, the DSD samples
		   are stuffed inside fake 32 bit samples */
		sample_format = SampleFormat::S32;

	dop = params.dop && sample_format == SampleFormat::DSD;
	if (dop)
		/* after the conversion to DoP, the DSD
		   samples are stuffed inside fake 24 bit samples */
		sample_format = SampleFormat::S24_P32;
#endif

	shift8 = params.shift8 && sample_format == SampleFormat::S24_P32;
	pack24 = params.pack24 && sample_format == SampleFormat::S24_P32;

	assert(!shift8 || !pack24);

	reverse_endian = 0;
	if (params.reverse_endian) {
		size_t sample_size = pack24
			? 3
			: sample_format_size(sample_format);
		assert(sample_size <= 0xff);

		if (sample_size > 1)
			reverse_endian = sample_size;
	}
}

size_t
PcmExport::GetFrameSize(const AudioFormat &audio_format) const noexcept
{
	if (pack24)
		/* packed 24 bit samples (3 bytes per sample) */
		return audio_format.channels * 3;

#ifdef ENABLE_DSD
	if (dsd_u16)
		return channels * 2;

	if (dsd_u32)
		return channels * 4;

	if (dop)
		/* the DSD-over-USB draft says that DSD 1-bit samples
		   are enclosed within 24 bit samples, and MPD's
		   representation of 24 bit is padded to 32 bit (4
		   bytes per sample) */
		return channels * 4;
#endif

	return audio_format.GetFrameSize();
}

unsigned
PcmExport::Params::CalcOutputSampleRate(unsigned sample_rate) const noexcept
{
#ifdef ENABLE_DSD
	if (dsd_u16)
		/* DSD_U16 combines two 8-bit "samples" in one 16-bit
		   "sample" */
		sample_rate /= 2;

	if (dsd_u32)
		/* DSD_U32 combines four 8-bit "samples" in one 32-bit
		   "sample" */
		sample_rate /= 4;

	if (dop)
		/* DoP packs two 8-bit "samples" in one 24-bit
		   "sample" */
		sample_rate /= 2;
#endif

	return sample_rate;
}

unsigned
PcmExport::Params::CalcInputSampleRate(unsigned sample_rate) const noexcept
{
#ifdef ENABLE_DSD
	if (dsd_u16)
		sample_rate *= 2;

	if (dsd_u32)
		sample_rate *= 4;

	if (dop)
		sample_rate *= 2;
#endif

	return sample_rate;
}

ConstBuffer<void>
PcmExport::Export(ConstBuffer<void> data) noexcept
{
	if (alsa_channel_order != SampleFormat::UNDEFINED)
		data = ToAlsaChannelOrder(order_buffer, data,
					  alsa_channel_order, channels);

#ifdef ENABLE_DSD
	if (dsd_u16)
		data = Dsd8To16(dop_buffer, channels,
				ConstBuffer<uint8_t>::FromVoid(data))
			.ToVoid();

	if (dsd_u32)
		data = Dsd8To32(dop_buffer, channels,
				ConstBuffer<uint8_t>::FromVoid(data))
			.ToVoid();

	if (dop)
		data = pcm_dsd_to_dop(dop_buffer, channels,
				      ConstBuffer<uint8_t>::FromVoid(data))
			.ToVoid();
#endif

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
PcmExport::CalcSourceSize(size_t size) const noexcept
{
	if (pack24)
		/* 32 bit to 24 bit conversion (4 to 3 bytes) */
		size = (size / 3) * 4;

#ifdef ENABLE_DSD
	if (dop)
		/* DoP doubles the transport size */
		size /= 2;
#endif

	return size;
}
