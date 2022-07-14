/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "PcmFormat.hxx"
#include "Buffer.hxx"
#include "Traits.hxx"
#include "FloatConvert.hxx"
#include "ShiftConvert.hxx"
#include "util/SpanCast.hxx"
#include "util/TransformN.hxx"

#include "Dither.cxx" // including the .cxx file to get inlined templates

/**
 * Wrapper for a class that converts one sample at a time into one
 * that converts a buffer at a time.
 */
template<typename C>
struct PerSampleConvert : C {
	using SrcTraits = typename C::SrcTraits;
	using DstTraits = typename C::DstTraits;

	void Convert(typename DstTraits::pointer gcc_restrict out,
		     typename SrcTraits::const_pointer gcc_restrict in,
		     size_t n) const {
		transform_n(in, n, out, C::Convert);
	}
};

struct Convert8To16
	: PerSampleConvert<LeftShiftSampleConvert<SampleFormat::S8,
						  SampleFormat::S16>> {};

struct Convert24To16 {
	using SrcTraits = SampleTraits<SampleFormat::S24_P32>;
	using DstTraits = SampleTraits<SampleFormat::S16>;

	PcmDither &dither;

	explicit Convert24To16(PcmDither &_dither):dither(_dither) {}

	void Convert(int16_t *out, const int32_t *in, size_t n) {
		dither.Dither24To16(out, in, in + n);
	}
};

struct Convert32To16 {
	using SrcTraits = SampleTraits<SampleFormat::S32>;
	using DstTraits = SampleTraits<SampleFormat::S16>;

	PcmDither &dither;

	explicit Convert32To16(PcmDither &_dither):dither(_dither) {}

	void Convert(int16_t *out, const int32_t *in, size_t n) {
		dither.Dither32To16(out, in, in + n);
	}
};

template<SampleFormat F, class Traits=SampleTraits<F>>
struct PortableFloatToInteger
	: PerSampleConvert<FloatToIntegerSampleConvert<F, Traits>> {};

template<SampleFormat F, class Traits=SampleTraits<F>>
struct FloatToInteger : PortableFloatToInteger<F, Traits> {};

/**
 * A template class that attempts to use the "optimized" algorithm for
 * large portions of the buffer, and calls the "portable" algorithm"
 * for the rest when the last block is not full.
 */
template<typename Optimized, typename Portable>
class GlueOptimizedConvert : Optimized, Portable {
public:
	using SrcTraits = typename Portable::SrcTraits;
	using DstTraits = typename Portable::DstTraits;

	void Convert(typename DstTraits::pointer out,
		     typename SrcTraits::const_pointer in,
		     size_t n) const {
		Optimized::Convert(out, in, n);

		/* use the "portable" algorithm for the trailing
		   samples */
		size_t remaining = n % Optimized::BLOCK_SIZE;
		size_t done = n - remaining;
		Portable::Convert(out + done, in + done, remaining);
	}
};

#ifdef __ARM_NEON__
#include "Neon.hxx"

template<>
struct FloatToInteger<SampleFormat::S16, SampleTraits<SampleFormat::S16>>
	: GlueOptimizedConvert<NeonFloatTo16,
			       PortableFloatToInteger<SampleFormat::S16>> {};

#endif

template<class C>
static std::span<const typename C::DstTraits::value_type>
AllocateConvert(PcmBuffer &buffer, C convert,
		std::span<const typename C::SrcTraits::value_type> src)
{
	auto dest = buffer.GetT<typename C::DstTraits::value_type>(src.size());
	convert.Convert(dest, src.data(), src.size());
	return { dest, src.size() };
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static std::span<const typename Traits::value_type>
AllocateFromFloat(PcmBuffer &buffer, std::span<const float> src)
{
	return AllocateConvert(buffer, FloatToInteger<F, Traits>(), src);
}

static std::span<const int16_t>
pcm_allocate_8_to_16(PcmBuffer &buffer, std::span<const int8_t> src)
{
	return AllocateConvert(buffer, Convert8To16(), src);
}

static std::span<const int16_t>
pcm_allocate_24p32_to_16(PcmBuffer &buffer, PcmDither &dither,
			 std::span<const int32_t> src)
{
	return AllocateConvert(buffer, Convert24To16(dither), src);
}

static std::span<const int16_t>
pcm_allocate_32_to_16(PcmBuffer &buffer, PcmDither &dither,
		      std::span<const int32_t> src)
{
	return AllocateConvert(buffer, Convert32To16(dither), src);
}

static std::span<const int16_t>
pcm_allocate_float_to_16(PcmBuffer &buffer, std::span<const float> src)
{
	return AllocateFromFloat<SampleFormat::S16>(buffer, src);
}

std::span<const int16_t>
pcm_convert_to_16(PcmBuffer &buffer, PcmDither &dither,
		  SampleFormat src_format, std::span<const std::byte> src) noexcept
{
	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_16(buffer,
					    FromBytesStrict<const int8_t>(src));

	case SampleFormat::S16:
		return FromBytesStrict<const int16_t>(src);

	case SampleFormat::S24_P32:
		return pcm_allocate_24p32_to_16(buffer, dither,
						FromBytesStrict<const int32_t>(src));

	case SampleFormat::S32:
		return pcm_allocate_32_to_16(buffer, dither,
					     FromBytesStrict<const int32_t>(src));

	case SampleFormat::FLOAT:
		return pcm_allocate_float_to_16(buffer,
						FromBytesStrict<const float>(src));
	}

	return {};
}

struct Convert8To24
	: PerSampleConvert<LeftShiftSampleConvert<SampleFormat::S8,
						  SampleFormat::S24_P32>> {};

struct Convert16To24
	: PerSampleConvert<LeftShiftSampleConvert<SampleFormat::S16,
						  SampleFormat::S24_P32>> {};

static std::span<const int32_t>
pcm_allocate_8_to_24(PcmBuffer &buffer, std::span<const int8_t> src)
{
	return AllocateConvert(buffer, Convert8To24(), src);
}

static std::span<const int32_t>
pcm_allocate_16_to_24(PcmBuffer &buffer, std::span<const int16_t> src)
{
	return AllocateConvert(buffer, Convert16To24(), src);
}

struct Convert32To24
	: PerSampleConvert<RightShiftSampleConvert<SampleFormat::S32,
						   SampleFormat::S24_P32>> {};

static std::span<const int32_t>
pcm_allocate_32_to_24(PcmBuffer &buffer, std::span<const int32_t> src)
{
	return AllocateConvert(buffer, Convert32To24(), src);
}

static std::span<const int32_t>
pcm_allocate_float_to_24(PcmBuffer &buffer, std::span<const float> src)
{
	return AllocateFromFloat<SampleFormat::S24_P32>(buffer, src);
}

std::span<const int32_t>
pcm_convert_to_24(PcmBuffer &buffer,
		  SampleFormat src_format, std::span<const std::byte> src) noexcept
{
	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_24(buffer,
					    FromBytesStrict<const int8_t>(src));

	case SampleFormat::S16:
		return pcm_allocate_16_to_24(buffer,
					     FromBytesStrict<const int16_t>(src));

	case SampleFormat::S24_P32:
		return FromBytesStrict<const int32_t>(src);

	case SampleFormat::S32:
		return pcm_allocate_32_to_24(buffer,
					     FromBytesStrict<const int32_t>(src));

	case SampleFormat::FLOAT:
		return pcm_allocate_float_to_24(buffer,
						FromBytesStrict<const float>(src));
	}

	return {};
}

struct Convert8To32
	: PerSampleConvert<LeftShiftSampleConvert<SampleFormat::S8,
						  SampleFormat::S32>> {};

struct Convert16To32
	: PerSampleConvert<LeftShiftSampleConvert<SampleFormat::S16,
						  SampleFormat::S32>> {};

struct Convert24To32
	: PerSampleConvert<LeftShiftSampleConvert<SampleFormat::S24_P32,
						  SampleFormat::S32>> {};

static std::span<const int32_t>
pcm_allocate_8_to_32(PcmBuffer &buffer, std::span<const int8_t> src)
{
	return AllocateConvert(buffer, Convert8To32(), src);
}

static std::span<const int32_t>
pcm_allocate_16_to_32(PcmBuffer &buffer, std::span<const int16_t> src)
{
	return AllocateConvert(buffer, Convert16To32(), src);
}

static std::span<const int32_t>
pcm_allocate_24p32_to_32(PcmBuffer &buffer, std::span<const int32_t> src)
{
	return AllocateConvert(buffer, Convert24To32(), src);
}

static std::span<const int32_t>
pcm_allocate_float_to_32(PcmBuffer &buffer, std::span<const float> src)
{
	return AllocateFromFloat<SampleFormat::S32>(buffer, src);
}

std::span<const int32_t>
pcm_convert_to_32(PcmBuffer &buffer,
		  SampleFormat src_format, std::span<const std::byte> src) noexcept
{
	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_32(buffer,
					    FromBytesStrict<const int8_t>(src));

	case SampleFormat::S16:
		return pcm_allocate_16_to_32(buffer,
					     FromBytesStrict<const int16_t>(src));

	case SampleFormat::S24_P32:
		return pcm_allocate_24p32_to_32(buffer,
						FromBytesStrict<const int32_t>(src));

	case SampleFormat::S32:
		return FromBytesStrict<const int32_t>(src);

	case SampleFormat::FLOAT:
		return pcm_allocate_float_to_32(buffer,
						FromBytesStrict<const float>(src));
	}

	return {};
}

struct Convert8ToFloat
	: PerSampleConvert<IntegerToFloatSampleConvert<SampleFormat::S8>> {};

struct Convert16ToFloat
	: PerSampleConvert<IntegerToFloatSampleConvert<SampleFormat::S16>> {};

struct Convert24ToFloat
	: PerSampleConvert<IntegerToFloatSampleConvert<SampleFormat::S24_P32>> {};

struct Convert32ToFloat
	: PerSampleConvert<IntegerToFloatSampleConvert<SampleFormat::S32>> {};

static std::span<const float>
pcm_allocate_8_to_float(PcmBuffer &buffer, std::span<const int8_t> src)
{
	return AllocateConvert(buffer, Convert8ToFloat(), src);
}

static std::span<const float>
pcm_allocate_16_to_float(PcmBuffer &buffer, std::span<const int16_t> src)
{
	return AllocateConvert(buffer, Convert16ToFloat(), src);
}

static std::span<const float>
pcm_allocate_24p32_to_float(PcmBuffer &buffer, std::span<const int32_t> src)
{
	return AllocateConvert(buffer, Convert24ToFloat(), src);
}

static std::span<const float>
pcm_allocate_32_to_float(PcmBuffer &buffer, std::span<const int32_t> src)
{
	return AllocateConvert(buffer, Convert32ToFloat(), src);
}

std::span<const float>
pcm_convert_to_float(PcmBuffer &buffer,
		     SampleFormat src_format, std::span<const std::byte> src) noexcept
{
	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_float(buffer,
					       FromBytesStrict<const int8_t>(src));

	case SampleFormat::S16:
		return pcm_allocate_16_to_float(buffer,
					       FromBytesStrict<const int16_t>(src));

	case SampleFormat::S32:
		return pcm_allocate_32_to_float(buffer,
					       FromBytesStrict<const int32_t>(src));

	case SampleFormat::S24_P32:
		return pcm_allocate_24p32_to_float(buffer,
						   FromBytesStrict<const int32_t>(src));

	case SampleFormat::FLOAT:
		return FromBytesStrict<const float>(src);
	}

	return {};
}
