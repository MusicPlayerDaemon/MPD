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

#include "FallbackResampler.hxx"

#include <cassert>

AudioFormat
FallbackPcmResampler::Open(AudioFormat &af, unsigned new_sample_rate)
{
	assert(af.IsValid());
	assert(audio_valid_sample_rate(new_sample_rate));

	switch (af.format) {
	case SampleFormat::UNDEFINED:
		assert(false);
		gcc_unreachable();

	case SampleFormat::S8:
		af.format = SampleFormat::S16;
		break;

	case SampleFormat::S16:
	case SampleFormat::FLOAT:
	case SampleFormat::S24_P32:
	case SampleFormat::S32:
		break;

	case SampleFormat::DSD:
		af.format = SampleFormat::FLOAT;
		break;
	}

	format = af;
	out_rate = new_sample_rate;

	AudioFormat result = af;
	result.sample_rate = new_sample_rate;
	return result;
}

void
FallbackPcmResampler::Close() noexcept
{
}

template<typename T>
static ConstBuffer<T>
pcm_resample_fallback(PcmBuffer &buffer,
		      unsigned channels,
		      unsigned src_rate,
		      ConstBuffer<T> src,
		      unsigned dest_rate) noexcept
{
	unsigned dest_pos = 0;
	unsigned src_frames = src.size / channels;
	unsigned dest_frames =
		(src_frames * dest_rate + src_rate - 1) / src_rate;
	unsigned dest_samples = dest_frames * channels;
	size_t dest_size = dest_samples * sizeof(*src.data);
	T *dest_buffer = (T *)buffer.Get(dest_size);

	assert((src.size % channels) == 0);

	switch (channels) {
	case 1:
		while (dest_pos < dest_samples) {
			unsigned src_pos = dest_pos * src_rate / dest_rate;

			dest_buffer[dest_pos++] = src[src_pos];
		}
		break;
	case 2:
		while (dest_pos < dest_samples) {
			unsigned src_pos = dest_pos * src_rate / dest_rate;
			src_pos &= ~1;

			dest_buffer[dest_pos++] = src[src_pos];
			dest_buffer[dest_pos++] = src[src_pos + 1];
		}
		break;
	}

	return { dest_buffer, dest_samples };
}

template<typename T>
static ConstBuffer<void>
pcm_resample_fallback_void(PcmBuffer &buffer,
			   unsigned channels,
			   unsigned src_rate,
			   ConstBuffer<void> src,
			   unsigned dest_rate) noexcept
{
	const auto typed_src = ConstBuffer<T>::FromVoid(src);
	return pcm_resample_fallback(buffer, channels, src_rate, typed_src,
				     dest_rate).ToVoid();
}

ConstBuffer<void>
FallbackPcmResampler::Resample(ConstBuffer<void> src)
{
	switch (format.format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::S8:
	case SampleFormat::DSD:
		assert(false);
		gcc_unreachable();

	case SampleFormat::S16:
		return pcm_resample_fallback_void<int16_t>(buffer,
							   format.channels,
							   format.sample_rate,
							   src,
							   out_rate);

	case SampleFormat::FLOAT:
	case SampleFormat::S24_P32:
	case SampleFormat::S32:
		return pcm_resample_fallback_void<int32_t>(buffer,
							   format.channels,
							   format.sample_rate,
							   src,
							   out_rate);
	}

	assert(false);
	gcc_unreachable();
}
