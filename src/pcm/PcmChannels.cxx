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

#include "PcmChannels.hxx"
#include "ChannelDefs.hxx"
#include "Buffer.hxx"
#include "Silence.hxx"
#include "Traits.hxx"
#include "util/ConstBuffer.hxx"
#include "util/WritableBuffer.hxx"

#include <array>
#include <algorithm>
#include <cassert>

template<typename D, typename S>
static void
MonoToStereo(D dest, S src, S end) noexcept
{
	while (src != end) {
		const auto value = *src++;

		*dest++ = value;
		*dest++ = value;
	}

}

template<SampleFormat F, class Traits=SampleTraits<F>>
static typename Traits::value_type
StereoToMono(typename Traits::value_type _a,
	     typename Traits::value_type _b) noexcept
{
	typename Traits::sum_type a(_a);
	typename Traits::sum_type b(_b);

	return typename Traits::value_type((a + b) / 2);
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static typename Traits::pointer
StereoToMono(typename Traits::pointer dest,
	     typename Traits::const_pointer src,
	     typename Traits::const_pointer end) noexcept
{
	while (src != end) {
		const auto a = *src++;
		const auto b = *src++;

		*dest++ = StereoToMono<F, Traits>(a, b);
	}

	return dest;
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static typename Traits::pointer
NToStereo(typename Traits::pointer dest,
	  unsigned src_channels,
	  typename Traits::const_pointer src,
	  typename Traits::const_pointer end) noexcept
{
	assert((end - src) % src_channels == 0);

	while (src != end) {
		typename Traits::sum_type sum = *src++;
		for (unsigned c = 1; c < src_channels; ++c)
			sum += *src++;

		typename Traits::value_type value(sum / int(src_channels));

		/* TODO: this is actually only mono ... */
		*dest++ = value;
		*dest++ = value;
	}

	return dest;
}

/**
 * Convert stereo to N channels (where N > 2).  Left and right map to
 * the first two channels (front left and front right), and the
 * remaining (surround) channels are filled with silence.
 */
template<SampleFormat F, class Traits=SampleTraits<F>>
static typename Traits::pointer
StereoToN(typename Traits::pointer dest,
	  unsigned dest_channels,
	  typename Traits::const_pointer src,
	  typename Traits::const_pointer end) noexcept
{
	assert(dest_channels > 2);
	assert((end - src) % 2 == 0);

	std::array<typename Traits::value_type, MAX_CHANNELS - 2> silence;
	PcmSilence({&silence.front(), sizeof(silence)}, F);

	while (src != end) {
		/* copy left/right to front-left/front-right, which is
		   the first two channels in all multi-channel
		   configurations **/
		*dest++ = *src++;
		*dest++ = *src++;

		/* all other channels are silent */
		dest = std::copy_n(silence.begin(), dest_channels - 2, dest);
	}

	return dest;
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static typename Traits::pointer
NToM(typename Traits::pointer dest,
     unsigned dest_channels,
     unsigned src_channels,
     typename Traits::const_pointer src,
     typename Traits::const_pointer end) noexcept
{
	assert((end - src) % src_channels == 0);

	while (src != end) {
		typename Traits::sum_type sum = *src++;
		for (unsigned c = 1; c < src_channels; ++c)
			sum += *src++;

		typename Traits::value_type value(sum / int(src_channels));

		/* TODO: this is actually only mono ... */
		for (unsigned c = 0; c < dest_channels; ++c)
			*dest++ = value;
	}

	return dest;
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static ConstBuffer<typename Traits::value_type>
ConvertChannels(PcmBuffer &buffer,
		unsigned dest_channels,
		unsigned src_channels,
		ConstBuffer<typename Traits::value_type> src) noexcept
{
	assert(src.size % src_channels == 0);

	const size_t dest_size = src.size / src_channels * dest_channels;
	auto dest = buffer.GetT<typename Traits::value_type>(dest_size);

	if (src_channels == 1 && dest_channels == 2)
		MonoToStereo(dest, src.begin(), src.end());
	else if (src_channels == 2 && dest_channels == 1)
		StereoToMono<F>(dest, src.begin(), src.end());
	else if (dest_channels == 2)
		NToStereo<F>(dest, src_channels, src.begin(), src.end());
	else if (src_channels == 2 && dest_channels > 2)
		StereoToN<F, Traits>(dest, dest_channels,
				     src.begin(), src.end());
	else
		NToM<F>(dest, dest_channels,
			src_channels, src.begin(), src.end());

	return { dest, dest_size };
}

ConstBuffer<int16_t>
pcm_convert_channels_16(PcmBuffer &buffer,
			unsigned dest_channels,
			unsigned src_channels,
			ConstBuffer<int16_t> src) noexcept
{
	return ConvertChannels<SampleFormat::S16>(buffer, dest_channels,
						  src_channels, src);
}

ConstBuffer<int32_t>
pcm_convert_channels_24(PcmBuffer &buffer,
			unsigned dest_channels,
			unsigned src_channels,
			ConstBuffer<int32_t> src) noexcept
{
	return ConvertChannels<SampleFormat::S24_P32>(buffer, dest_channels,
						      src_channels, src);
}

ConstBuffer<int32_t>
pcm_convert_channels_32(PcmBuffer &buffer,
			unsigned dest_channels,
			unsigned src_channels,
			ConstBuffer<int32_t> src) noexcept
{
	return ConvertChannels<SampleFormat::S32>(buffer, dest_channels,
						  src_channels, src);
}

ConstBuffer<float>
pcm_convert_channels_float(PcmBuffer &buffer,
			   unsigned dest_channels,
			   unsigned src_channels,
			   ConstBuffer<float> src) noexcept
{
	return ConvertChannels<SampleFormat::FLOAT>(buffer, dest_channels,
						    src_channels, src);
}
