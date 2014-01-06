/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "util/ConstBuffer.hxx"
#include "util/WritableBuffer.hxx"

#include "PcmDither.cxx" // including the .cxx file to get inlined templates

template<typename T>
static inline ConstBuffer<T>
ToConst(WritableBuffer<T> b)
{
	return { b.data, b.size };
}

static void
pcm_convert_8_to_16(int16_t *out, const int8_t *in, size_t n)
{
	for (size_t i = 0; i != n; ++i)
		out[i] = in[i] << 8;
}

static void
pcm_convert_24_to_16(PcmDither &dither,
		     int16_t *out, const int32_t *in, const int32_t *in_end)
{
	dither.Dither24To16(out, in, in_end);
}

static void
pcm_convert_32_to_16(PcmDither &dither,
		     int16_t *out, const int32_t *in, const int32_t *in_end)
{
	dither.Dither32To16(out, in, in_end);
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static void
ConvertFromFloat(typename Traits::pointer_type dest,
		 const float *src, size_t n)
{
	constexpr auto bits = Traits::BITS;

	const float factor = 1 << (bits - 1);

	for (size_t i = 0; i != n; ++i) {
		typename Traits::long_type sample(src[i] * factor);
		dest[i] = PcmClamp<F, Traits>(sample);
	}
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static typename Traits::pointer_type
AllocateFromFloat(PcmBuffer &buffer, const float *src, size_t src_size,
		  size_t *dest_size_r)
{
	constexpr size_t src_sample_size = sizeof(*src);
	assert(src_size % src_sample_size == 0);

	const size_t num_samples = src_size / src_sample_size;
	*dest_size_r = num_samples * sizeof(typename Traits::value_type);
	auto dest = (typename Traits::pointer_type)buffer.Get(*dest_size_r);
	ConvertFromFloat<F, Traits>(dest, src, src_size / sizeof(*src));
	return dest;
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static WritableBuffer<typename Traits::value_type>
AllocateFromFloat(PcmBuffer &buffer, ConstBuffer<float> src)
{
	auto dest = buffer.GetT<typename Traits::value_type>(src.size);
	ConvertFromFloat<F, Traits>(dest, src.data, src.size);
	return { dest, src.size };
}

static ConstBuffer<int16_t>
pcm_allocate_8_to_16(PcmBuffer &buffer, ConstBuffer<int8_t> src)
{
	auto dest = buffer.GetT<int16_t>(src.size);
	pcm_convert_8_to_16(dest, src.data, src.size);
	return { dest, src.size };
}

static ConstBuffer<int16_t>
pcm_allocate_24p32_to_16(PcmBuffer &buffer, PcmDither &dither,
			 ConstBuffer<int32_t> src)
{
	auto dest = buffer.GetT<int16_t>(src.size);
	pcm_convert_24_to_16(dither, dest, src.data, src.end());
	return { dest, src.size };
}

static ConstBuffer<int16_t>
pcm_allocate_32_to_16(PcmBuffer &buffer, PcmDither &dither,
		      ConstBuffer<int32_t> src)
{
	auto dest = buffer.GetT<int16_t>(src.size);
	pcm_convert_32_to_16(dither, dest, src.data, src.end());
	return { dest, src.size };
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

static void
pcm_convert_8_to_24(int32_t *out, const int8_t *in, size_t n)
{
	for (size_t i = 0; i != n; ++i)
		out[i] = in[i] << 16;
}

static void
pcm_convert_16_to_24(int32_t *out, const int16_t *in, size_t n)
{
	for (size_t i = 0; i != n; ++i)
		out[i] = in[i] << 8;
}

static void
pcm_convert_32_to_24(int32_t *gcc_restrict out,
		     const int32_t *gcc_restrict in,
		     size_t n)
{
	for (size_t i = 0; i != n; ++i)
		out[i] = in[i] >> 8;
}

static ConstBuffer<int32_t>
pcm_allocate_8_to_24(PcmBuffer &buffer, ConstBuffer<int8_t> src)
{
	auto dest = buffer.GetT<int32_t>(src.size);
	pcm_convert_8_to_24(dest, src.data, src.size);
	return { dest, src.size };
}

static ConstBuffer<int32_t>
pcm_allocate_16_to_24(PcmBuffer &buffer, ConstBuffer<int16_t> src)
{
	auto dest = buffer.GetT<int32_t>(src.size);
	pcm_convert_16_to_24(dest, src.data, src.size);
	return { dest, src.size };
}

static ConstBuffer<int32_t>
pcm_allocate_32_to_24(PcmBuffer &buffer, ConstBuffer<int32_t> src)
{
	auto dest = buffer.GetT<int32_t>(src.size);
	pcm_convert_32_to_24(dest, src.data, src.size);
	return { dest, src.size };
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

static void
pcm_convert_8_to_32(int32_t *out, const int8_t *in, size_t n)
{
	for (size_t i = 0; i != n; ++i)
		out[i] = in[i] << 24;
}

static void
pcm_convert_16_to_32(int32_t *out, const int16_t *in, size_t n)
{
	for (size_t i = 0; i != n; ++i)
		out[i] = in[i] << 16;
}

static void
pcm_convert_24_to_32(int32_t *gcc_restrict out,
		     const int32_t *gcc_restrict in,
		     size_t n)
{
	for (size_t i = 0; i != n; ++i)
		out[i] = in[i] << 8;
}

static ConstBuffer<int32_t>
pcm_allocate_8_to_32(PcmBuffer &buffer, ConstBuffer<int8_t> src)
{
	auto dest = buffer.GetT<int32_t>(src.size);
	pcm_convert_8_to_32(dest, src.data, src.size);
	return { dest, src.size };
}

static ConstBuffer<int32_t>
pcm_allocate_16_to_32(PcmBuffer &buffer, ConstBuffer<int16_t> src)
{
	auto dest = buffer.GetT<int32_t>(src.size);
	pcm_convert_16_to_32(dest, src.data, src.size);
	return { dest, src.size };
}

static ConstBuffer<int32_t>
pcm_allocate_24p32_to_32(PcmBuffer &buffer, ConstBuffer<int32_t> src)
{
	auto dest = buffer.GetT<int32_t>(src.size);
	pcm_convert_24_to_32(dest, src.data, src.size);
	return { dest, src.size };
}

static ConstBuffer<int32_t>
pcm_allocate_float_to_32(PcmBuffer &buffer, ConstBuffer<float> src)
{
	/* convert to S24_P32 first */
	auto dest = pcm_allocate_float_to_24(buffer, src);

	/* convert to 32 bit in-place */
	pcm_convert_24_to_32(dest.data, dest.data, src.size);
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

template<SampleFormat F, class Traits=SampleTraits<F>>
static void
ConvertToFloat(float *dest,
	       typename Traits::const_pointer_type src,
	       size_t n)
{
	constexpr float factor = 0.5 / (1 << (Traits::BITS - 2));
	for (size_t i = 0; i != n; ++i)
		dest[i] = float(src[i]) * factor;
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static ConstBuffer<float>
AllocateToFloat(PcmBuffer &buffer,
		ConstBuffer<typename Traits::value_type> src)
{
	float *dest = buffer.GetT<float>(src.size);
	ConvertToFloat<F, Traits>(dest, src.data, src.size);
	return { dest, src.size };
}

static ConstBuffer<float>
pcm_allocate_8_to_float(PcmBuffer &buffer, ConstBuffer<int8_t> src)
{
	return AllocateToFloat<SampleFormat::S8>(buffer, src);
}

static ConstBuffer<float>
pcm_allocate_16_to_float(PcmBuffer &buffer, ConstBuffer<int16_t> src)
{
	return AllocateToFloat<SampleFormat::S16>(buffer, src);
}

static ConstBuffer<float>
pcm_allocate_24p32_to_float(PcmBuffer &buffer, ConstBuffer<int32_t> src)
{
	return AllocateToFloat<SampleFormat::S24_P32>(buffer, src);
}

static ConstBuffer<float>
pcm_allocate_32_to_float(PcmBuffer &buffer, ConstBuffer<int32_t> src)
{
	return AllocateToFloat<SampleFormat::S32>(buffer, src);
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
