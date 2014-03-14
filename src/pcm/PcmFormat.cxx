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
#include "PcmFormat.hxx"
#include "PcmBuffer.hxx"
#include "PcmUtils.hxx"
#include "Traits.hxx"
#include "FloatConvert.hxx"
#include "ShiftConvert.hxx"
#include "util/ConstBuffer.hxx"
#include "util/WritableBuffer.hxx"

#include "PcmDither.cxx" // including the .cxx file to get inlined templates

template<typename T>
static inline ConstBuffer<T>
ToConst(WritableBuffer<T> b)
{
	return { b.data, b.size };
}

/**
 * Wrapper for a class that converts one sample at a time into one
 * that converts a buffer at a time.
 */
template<typename C>
struct PerSampleConvert : C {
	typedef typename C::SrcTraits SrcTraits;
	typedef typename C::DstTraits DstTraits;

	void Convert(typename DstTraits::pointer_type gcc_restrict out,
		     typename SrcTraits::const_pointer_type gcc_restrict in,
		     size_t n) const {
		for (size_t i = 0; i != n; ++i)
			out[i] = C::Convert(in[i]);
	}
};

struct Convert8To16
	: PerSampleConvert<LeftShiftSampleConvert<SampleFormat::S8,
						  SampleFormat::S16>> {};

struct Convert24To16 {
	typedef SampleTraits<SampleFormat::S24_P32> SrcTraits;
	typedef SampleTraits<SampleFormat::S16> DstTraits;

	PcmDither &dither;

	Convert24To16(PcmDither &_dither):dither(_dither) {}

	void Convert(int16_t *out, const int32_t *in, size_t n) {
		dither.Dither24To16(out, in, in + n);
	}
};

struct Convert32To16 {
	typedef SampleTraits<SampleFormat::S32> SrcTraits;
	typedef SampleTraits<SampleFormat::S16> DstTraits;

	PcmDither &dither;

	Convert32To16(PcmDither &_dither):dither(_dither) {}

	void Convert(int16_t *out, const int32_t *in, size_t n) {
		dither.Dither32To16(out, in, in + n);
	}
};

template<SampleFormat F, class Traits=SampleTraits<F>>
struct FloatToInteger
	: PerSampleConvert<FloatToIntegerSampleConvert<F, Traits>> {};

template<class C>
static WritableBuffer<typename C::DstTraits::value_type>
AllocateConvert(PcmBuffer &buffer, C convert,
		ConstBuffer<typename C::SrcTraits::value_type> src)
{
	auto dest = buffer.GetT<typename C::DstTraits::value_type>(src.size);
	convert.Convert(dest, src.data, src.size);
	return { dest, src.size };
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static WritableBuffer<typename Traits::value_type>
AllocateFromFloat(PcmBuffer &buffer, ConstBuffer<float> src)
{
	return AllocateConvert(buffer, FloatToInteger<F, Traits>(), src);
}

static ConstBuffer<int16_t>
pcm_allocate_8_to_16(PcmBuffer &buffer, ConstBuffer<int8_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert8To16(), src));
}

static ConstBuffer<int16_t>
pcm_allocate_24p32_to_16(PcmBuffer &buffer, PcmDither &dither,
			 ConstBuffer<int32_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert24To16(dither), src));
}

static ConstBuffer<int16_t>
pcm_allocate_32_to_16(PcmBuffer &buffer, PcmDither &dither,
		      ConstBuffer<int32_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert32To16(dither), src));
}

static ConstBuffer<int16_t>
pcm_allocate_float_to_16(PcmBuffer &buffer, ConstBuffer<float> src)
{
	return ToConst(AllocateFromFloat<SampleFormat::S16>(buffer, src));
}

ConstBuffer<int16_t>
pcm_convert_to_16(PcmBuffer &buffer, PcmDither &dither,
		  SampleFormat src_format, ConstBuffer<void> src)
{
	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_16(buffer,
					    ConstBuffer<int8_t>::FromVoid(src));

	case SampleFormat::S16:
		return ConstBuffer<int16_t>::FromVoid(src);

	case SampleFormat::S24_P32:
		return pcm_allocate_24p32_to_16(buffer, dither,
						ConstBuffer<int32_t>::FromVoid(src));

	case SampleFormat::S32:
		return pcm_allocate_32_to_16(buffer, dither,
					     ConstBuffer<int32_t>::FromVoid(src));

	case SampleFormat::FLOAT:
		return pcm_allocate_float_to_16(buffer,
						ConstBuffer<float>::FromVoid(src));
	}

	return nullptr;
}

struct Convert8To24
	: PerSampleConvert<LeftShiftSampleConvert<SampleFormat::S8,
						  SampleFormat::S24_P32>> {};

struct Convert16To24
	: PerSampleConvert<LeftShiftSampleConvert<SampleFormat::S16,
						  SampleFormat::S24_P32>> {};

static ConstBuffer<int32_t>
pcm_allocate_8_to_24(PcmBuffer &buffer, ConstBuffer<int8_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert8To24(), src));
}

static ConstBuffer<int32_t>
pcm_allocate_16_to_24(PcmBuffer &buffer, ConstBuffer<int16_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert16To24(), src));
}

struct Convert32To24
	: PerSampleConvert<RightShiftSampleConvert<SampleFormat::S32,
						   SampleFormat::S24_P32>> {};

static ConstBuffer<int32_t>
pcm_allocate_32_to_24(PcmBuffer &buffer, ConstBuffer<int32_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert32To24(), src));
}

static WritableBuffer<int32_t>
pcm_allocate_float_to_24(PcmBuffer &buffer, ConstBuffer<float> src)
{
	return AllocateFromFloat<SampleFormat::S24_P32>(buffer, src);
}

ConstBuffer<int32_t>
pcm_convert_to_24(PcmBuffer &buffer,
		  SampleFormat src_format, ConstBuffer<void> src)
{
	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_24(buffer,
					    ConstBuffer<int8_t>::FromVoid(src));

	case SampleFormat::S16:
		return pcm_allocate_16_to_24(buffer,
					     ConstBuffer<int16_t>::FromVoid(src));

	case SampleFormat::S24_P32:
		return ConstBuffer<int32_t>::FromVoid(src);

	case SampleFormat::S32:
		return pcm_allocate_32_to_24(buffer,
					     ConstBuffer<int32_t>::FromVoid(src));

	case SampleFormat::FLOAT:
		return ToConst(pcm_allocate_float_to_24(buffer,
							ConstBuffer<float>::FromVoid(src)));
	}

	return nullptr;
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

static ConstBuffer<int32_t>
pcm_allocate_8_to_32(PcmBuffer &buffer, ConstBuffer<int8_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert8To32(), src));
}

static ConstBuffer<int32_t>
pcm_allocate_16_to_32(PcmBuffer &buffer, ConstBuffer<int16_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert16To32(), src));
}

static ConstBuffer<int32_t>
pcm_allocate_24p32_to_32(PcmBuffer &buffer, ConstBuffer<int32_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert24To32(), src));
}

static ConstBuffer<int32_t>
pcm_allocate_float_to_32(PcmBuffer &buffer, ConstBuffer<float> src)
{
	/* convert to S24_P32 first */
	auto dest = pcm_allocate_float_to_24(buffer, src);

	/* convert to 32 bit in-place */
	Convert24To32().Convert(dest.data, dest.data, src.size);
	return ToConst(dest);
}

ConstBuffer<int32_t>
pcm_convert_to_32(PcmBuffer &buffer,
		  SampleFormat src_format, ConstBuffer<void> src)
{
	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_32(buffer,
					    ConstBuffer<int8_t>::FromVoid(src));

	case SampleFormat::S16:
		return pcm_allocate_16_to_32(buffer,
					     ConstBuffer<int16_t>::FromVoid(src));

	case SampleFormat::S24_P32:
		return pcm_allocate_24p32_to_32(buffer,
						ConstBuffer<int32_t>::FromVoid(src));

	case SampleFormat::S32:
		return ConstBuffer<int32_t>::FromVoid(src);

	case SampleFormat::FLOAT:
		return pcm_allocate_float_to_32(buffer,
						ConstBuffer<float>::FromVoid(src));
	}

	return nullptr;
}

struct Convert8ToFloat
	: PerSampleConvert<IntegerToFloatSampleConvert<SampleFormat::S8>> {};

struct Convert16ToFloat
	: PerSampleConvert<IntegerToFloatSampleConvert<SampleFormat::S16>> {};

struct Convert24ToFloat
	: PerSampleConvert<IntegerToFloatSampleConvert<SampleFormat::S24_P32>> {};

struct Convert32ToFloat
	: PerSampleConvert<IntegerToFloatSampleConvert<SampleFormat::S32>> {};

static ConstBuffer<float>
pcm_allocate_8_to_float(PcmBuffer &buffer, ConstBuffer<int8_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert8ToFloat(), src));
}

static ConstBuffer<float>
pcm_allocate_16_to_float(PcmBuffer &buffer, ConstBuffer<int16_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert16ToFloat(), src));
}

static ConstBuffer<float>
pcm_allocate_24p32_to_float(PcmBuffer &buffer, ConstBuffer<int32_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert24ToFloat(), src));
}

static ConstBuffer<float>
pcm_allocate_32_to_float(PcmBuffer &buffer, ConstBuffer<int32_t> src)
{
	return ToConst(AllocateConvert(buffer, Convert32ToFloat(), src));
}

ConstBuffer<float>
pcm_convert_to_float(PcmBuffer &buffer,
		     SampleFormat src_format, ConstBuffer<void> src)
{
	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_float(buffer,
					       ConstBuffer<int8_t>::FromVoid(src));

	case SampleFormat::S16:
		return pcm_allocate_16_to_float(buffer,
					       ConstBuffer<int16_t>::FromVoid(src));

	case SampleFormat::S32:
		return pcm_allocate_32_to_float(buffer,
					       ConstBuffer<int32_t>::FromVoid(src));

	case SampleFormat::S24_P32:
		return pcm_allocate_24p32_to_float(buffer,
						   ConstBuffer<int32_t>::FromVoid(src));

	case SampleFormat::FLOAT:
		return ConstBuffer<float>::FromVoid(src);
	}

	return nullptr;
}
