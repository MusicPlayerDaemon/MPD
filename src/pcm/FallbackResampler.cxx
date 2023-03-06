// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FallbackResampler.hxx"
#include "util/SpanCast.hxx"

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
static std::span<const T>
pcm_resample_fallback(PcmBuffer &buffer,
		      unsigned channels,
		      unsigned src_rate,
		      std::span<const T> src,
		      unsigned dest_rate) noexcept
{
	unsigned dest_pos = 0;
	unsigned src_frames = src.size() / channels;
	unsigned dest_frames =
		(src_frames * dest_rate + src_rate - 1) / src_rate;
	unsigned dest_samples = dest_frames * channels;
	size_t dest_size = dest_samples * sizeof(T);
	T *dest_buffer = (T *)buffer.Get(dest_size);

	assert((src.size() % channels) == 0);

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
static std::span<const std::byte>
pcm_resample_fallback_void(PcmBuffer &buffer,
			   unsigned channels,
			   unsigned src_rate,
			   std::span<const std::byte> src,
			   unsigned dest_rate) noexcept
{
	const auto typed_src = FromBytesStrict<const T>(src);
	return std::as_bytes(pcm_resample_fallback(buffer, channels, src_rate, typed_src,
						   dest_rate));
}

std::span<const std::byte>
FallbackPcmResampler::Resample(std::span<const std::byte> src)
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
