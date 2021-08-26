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

#ifndef MPD_AUDIO_FORMAT_HXX
#define MPD_AUDIO_FORMAT_HXX

#include "pcm/SampleFormat.hxx" // IWYU pragma: export
#include "pcm/ChannelDefs.hxx" // IWYU pragma: export

#include <cstddef>
#include <cstdint>

template<std::size_t CAPACITY> class StringBuffer;

/**
 * This structure describes the format of a raw PCM stream.
 */
struct AudioFormat {
	/**
	 * The sample rate in Hz.  A better name for this attribute is
	 * "frame rate", because technically, you have two samples per
	 * frame in stereo sound.
	 */
	uint32_t sample_rate;

	/**
	 * The format samples are stored in.  See the #sample_format
	 * enum for valid values.
	 */
	SampleFormat format;

	/**
	 * The number of channels.
	 *
	 * Channel order follows the FLAC convention
	 * (https://xiph.org/flac/format.html):
	 *
	 * - 1 channel: mono
	 * - 2 channels: left, right
	 * - 3 channels: left, right, center
	 * - 4 channels: front left, front right, back left, back right
	 * - 5 channels: front left, front right, front center, back/surround left, back/surround right
	 * - 6 channels: front left, front right, front center, LFE, back/surround left, back/surround right
	 * - 7 channels: front left, front right, front center, LFE, back center, side left, side right
	 * - 8 channels: front left, front right, front center, LFE, back left, back right, side left, side right
	 */
	uint8_t channels;

	AudioFormat() noexcept = default;

	constexpr AudioFormat(uint32_t _sample_rate,
			      SampleFormat _format, uint8_t _channels) noexcept
		:sample_rate(_sample_rate),
		 format(_format), channels(_channels) {}

	static constexpr AudioFormat Undefined() noexcept {
		return AudioFormat(0, SampleFormat::UNDEFINED,0);
	}

	/**
	 * Clears the object, i.e. sets all attributes to an undefined
	 * (invalid) value.
	 */
	void Clear() noexcept {
		sample_rate = 0;
		format = SampleFormat::UNDEFINED;
		channels = 0;
	}

	/**
	 * Checks whether the object has a defined value.
	 */
	constexpr bool IsDefined() const noexcept {
		return sample_rate != 0;
	}

	/**
	 * Checks whether the object is full, i.e. all attributes are
	 * defined.  This is more complete than IsDefined(), but
	 * slower.
	 */
	constexpr bool IsFullyDefined() const noexcept {
		return sample_rate != 0 && format != SampleFormat::UNDEFINED &&
			channels != 0;
	}

	/**
	 * Checks whether the object has at least one defined value.
	 */
	constexpr bool IsMaskDefined() const noexcept {
		return sample_rate != 0 || format != SampleFormat::UNDEFINED ||
			channels != 0;
	}

	bool IsValid() const noexcept;
	bool IsMaskValid() const noexcept;

	constexpr bool operator==(const AudioFormat other) const noexcept {
		return sample_rate == other.sample_rate &&
			format == other.format &&
			channels == other.channels;
	}

	constexpr bool operator!=(const AudioFormat other) const noexcept {
		return !(*this == other);
	}

	void ApplyMask(AudioFormat mask) noexcept;

	[[gnu::pure]]
	AudioFormat WithMask(AudioFormat mask) const noexcept {
		AudioFormat result = *this;
		result.ApplyMask(mask);
		return result;
	}

	[[gnu::pure]]
	bool MatchMask(AudioFormat mask) const noexcept {
		return WithMask(mask) == *this;
	}

	/**
	 * Returns the size of each (mono) sample in bytes.
	 */
	unsigned GetSampleSize() const noexcept;

	/**
	 * Returns the size of each full frame in bytes.
	 */
	unsigned GetFrameSize() const noexcept;

	template<typename D>
	constexpr auto TimeToFrames(D t) const noexcept {
		using Period = typename D::period;
		return ((t.count() * sample_rate) / Period::den) * Period::num;
	}

	template<typename D>
	constexpr std::size_t TimeToSize(D t) const noexcept {
		return std::size_t(std::size_t(TimeToFrames(t)) * GetFrameSize());
	}

	template<typename D>
	constexpr D FramesToTime(std::uintmax_t size) const noexcept {
		using Rep = typename D::rep;
		using Period = typename D::period;
		return D(((Rep(1) * size / Period::num) * Period::den) / sample_rate);
	}

	template<typename D>
	constexpr D SizeToTime(std::uintmax_t size) const noexcept {
		return FramesToTime<D>(size / GetFrameSize());
	}
};

/**
 * Checks whether the sample rate is valid.
 *
 * @param sample_rate the sample rate in Hz
 */
constexpr bool
audio_valid_sample_rate(unsigned sample_rate) noexcept
{
	return sample_rate > 0 && sample_rate < (1 << 30);
}

/**
 * Returns false if the format is not valid for playback with MPD.
 * This function performs some basic validity checks.
 */
inline bool
AudioFormat::IsValid() const noexcept
{
	return audio_valid_sample_rate(sample_rate) &&
		audio_valid_sample_format(format) &&
		audio_valid_channel_count(channels);
}

/**
 * Returns false if the format mask is not valid for playback with
 * MPD.  This function performs some basic validity checks.
 */
inline bool
AudioFormat::IsMaskValid() const noexcept
{
	return (sample_rate == 0 ||
		audio_valid_sample_rate(sample_rate)) &&
		(format == SampleFormat::UNDEFINED ||
		 audio_valid_sample_format(format)) &&
		(channels == 0 || audio_valid_channel_count(channels));
}

inline unsigned
AudioFormat::GetSampleSize() const noexcept
{
	return sample_format_size(format);
}

inline unsigned
AudioFormat::GetFrameSize() const noexcept
{
	return GetSampleSize() * channels;
}

/**
 * Renders the #AudioFormat object into a string, e.g. for printing
 * it in a log file.
 *
 * @param af the #AudioFormat object
 * @return the string buffer
 */
[[gnu::const]]
StringBuffer<24>
ToString(AudioFormat af) noexcept;

#endif
