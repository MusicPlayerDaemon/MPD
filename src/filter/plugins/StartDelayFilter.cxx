// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StartDelayFilter.hxx"
#include "pcm/AudioFormat.hxx"
#include "pcm/Prng.hxx"
#include "pcm/Silence.hxx"

#include <cstring>
#include <memory>
#include <utility>

static unsigned
GetSampleBits(const AudioFormat &format) noexcept
{
	if (format.format == SampleFormat::S24_P32)
		return 24;
	return format.GetSampleSize() * CHAR_BIT;
}

StartDelayFilter::StartDelayFilter(const AudioFormat &format,
			 std::chrono::milliseconds time,
			 std::uint8_t level,
			 bool _on_seek) noexcept
				:Filter(format),
				 delay_bytes(format.TimeToSize(time)),
				 initial_data(),
				 delay_bytes_read(0),
				 on_seek(_on_seek)
{
	const std::size_t frame_size = format.GetFrameSize();
	std::size_t buffer_frames = (delay_buffer.size() / frame_size);
	buffer_bytes = buffer_frames * frame_size;

	if (level > 0)
		CreateNoise(buffer_frames, level);
	else
		PcmSilence({ delay_buffer.data(), buffer_bytes }, format.format);
}

/**
 * Fill the noise buffer with pseudo-random values.
 * This sound is not intended to be heard, only to inform the device that the
 * stream is active. So we optimize resource usage by generating a short
 * sequence representing a square wave of fixed amplitude and randomly varying
 * wavelength.
 * @param format the desired format of the noise
 * @param level adjusts the amplitude of the noise
 */
void
StartDelayFilter::CreateNoise(std::size_t buffer_frames, std::uint8_t level) noexcept
{
	const unsigned sample_bits = GetSampleBits(out_audio_format);
	const std::size_t sample_size = out_audio_format.GetSampleSize();

	/* amplitude of the generated wave.
	 * For integer formats with a sample size less than 16 bits the
	 * amplitude is always 1 bit (-48.1 dBFS for 8 bits);
	 * For larger integer formats and floats the level is scaled to the
	 * range -90.3 dBFS to -42.1 dBFS  */
	const std::int32_t amplitude = sample_bits >= 16
		? std::int32_t(level) << (sample_bits - 16)
		: 1;
	const float famplitude = float(level) / 32768.0f;

	/* random number generator state */
	unsigned long rng = 0;

	/* the address within the byte array to write the generated frame. */
	auto dest = delay_buffer.data();

	for (std::size_t i = 0; i < buffer_frames; i++, dest += sample_size) {

		/* vary the sign randomly */
		rng = pcm_prng(rng);
		const bool negative = rng & 0x10000;

		/* write the sample value to the byte array */
		switch (out_audio_format.format) {
			case SampleFormat::S8: {
				const std::int8_t val = negative ? -amplitude : amplitude;
				std::memcpy(dest, &val, sizeof(val));
				break;
			}
			case SampleFormat::S16: {
				const std::int16_t val = negative ? -amplitude : amplitude;
				std::memcpy(dest, &val, sizeof(val));
				break;
			}
			case SampleFormat::S32: {
				const std::int32_t val = negative ? -amplitude : amplitude;
				std::memcpy(dest, &val, sizeof(val));
				break;
			}
			case SampleFormat::S24_P32: {
				const std::int32_t val = negative ?
					/* extend the sign bit */
					-amplitude | ~std::int32_t(0xffffff) :
					amplitude;
				std::memcpy(dest, &val, sizeof(val));
				break;
			}
			case SampleFormat::FLOAT: {
				const float val = negative ? -famplitude : famplitude;
				std::memcpy(dest, &val, sizeof(val));
				break;
			}
			default:
				std::unreachable();
		}

		/* copy this sample to each channel */
		for (auto c = 1; c < out_audio_format.channels; c++, dest += sample_size)
			std::memcpy(dest + sample_size, dest, sample_size);
	}
}

std::span<const std::byte>
StartDelayFilter::NextDelayChunk() noexcept
{
	auto next_chunk_size = delay_bytes - delay_bytes_read;
	if (next_chunk_size > buffer_bytes)
		next_chunk_size = buffer_bytes;
	delay_bytes_read += next_chunk_size;
	return std::span{delay_buffer.data(), next_chunk_size};

}

std::span<const std::byte>
StartDelayFilter::FilterPCM(std::span<const std::byte> src)
{
	if (delay_bytes_read < delay_bytes)
	{
		initial_data = src;
		return NextDelayChunk();
	}

	return src;
}

std::span<const std::byte>
StartDelayFilter::ReadMore()
{
	if (delay_bytes_read < delay_bytes)
		return NextDelayChunk();

	return std::exchange(initial_data, {});
}

std::span<const std::byte>
StartDelayFilter::Flush()
{
	return ReadMore();
}

void
StartDelayFilter::Reset() noexcept
{
	if (on_seek)
		delay_bytes_read = 0;
}
