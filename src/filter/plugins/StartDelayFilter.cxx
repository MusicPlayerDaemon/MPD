// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StartDelayFilter.hxx"
#include "pcm/AudioFormat.hxx"
#include "pcm/Prng.hxx"
#include "pcm/Silence.hxx"
#include "util/ByteSizes.hxx"
#include "util/SpanCast.hxx"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>

/* To limit resource usage */
static constexpr std::size_t DELAY_BUFFER_MAX_SIZE = 300_KiB;

template <typename T>
static void
Noise(const std::span<std::byte> &buffer, uint8_t channels, T amplitude) noexcept
{
	const auto typed_data(FromBytesFloor<T>(buffer));
	std::int32_t randval {0};
	unsigned repeats {0};

	auto f = [channels, amplitude, &randval, &repeats] () ->T {
		/* vary the sign randomly after repeating for each channel */
		if (++repeats > channels) {
			randval = pcm_prng(randval);
			repeats = 0;
		}
		return randval & 0x10000 ? -amplitude : amplitude;
	};

	std::ranges::generate(typed_data, f);
}

StartDelayFilter::StartDelayFilter(const AudioFormat &format,
			 std::chrono::milliseconds time,
			 std::uint8_t level,
			 bool _on_reset) noexcept
	:Filter(format),
	 delay_bytes(format.TimeToSize(time)),
	 on_reset(_on_reset)
{
	/* Round down the delay buffer size to a whole number of audio frames */
	const auto buffer_max_size = std::min(delay_bytes, DELAY_BUFFER_MAX_SIZE);
	const auto frame_size = format.GetFrameSize();
	const std::size_t buffer_frames = (buffer_max_size / frame_size);
	delay_buffer = AllocatedArray<std::byte>(buffer_frames * frame_size);

	/* This filter requires that delay buffer must not be zero length. */
	assert(delay_buffer.capacity() > 0);

	if (level > 0)
		CreateNoise(format, level);
	else
		PcmSilence(delay_buffer, format.format);
}

void StartDelayFilter::CreateNoise(const AudioFormat &format, uint8_t level) noexcept
{

	switch (format.format) {
		case SampleFormat::S8:
		{
			const std::int8_t amplitude = 1;
			Noise(delay_buffer, format.channels, amplitude);
			break;
		}
		case SampleFormat::S16:
		{
			const std::int16_t amplitude = level;
			Noise(delay_buffer, format.channels, amplitude);
			break;
		}
		case SampleFormat::S24_P32:
		{
			const std::int32_t amplitude = level << 8;
			Noise(delay_buffer, format.channels, amplitude);
			break;
		}
		case SampleFormat::S32:
		{
			const std::int32_t amplitude = level << 16;
			Noise(delay_buffer, format.channels, amplitude);
			break;
		}
		case SampleFormat::FLOAT: {
			const float amplitude = float(level) / 32768.0f;
			Noise(delay_buffer, format.channels, amplitude);
			break;
		}
		default:
			std::unreachable();
	}
}

std::span<const std::byte>
StartDelayFilter::NextDelayChunk() noexcept
{
	auto next_chunk_size = std::min(delay_bytes - delay_bytes_read, delay_buffer.size());
	delay_bytes_read += next_chunk_size;
	return std::span{delay_buffer}.first(next_chunk_size);
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
	initial_data = {};
	if (on_reset)
		delay_bytes_read = 0;
}
