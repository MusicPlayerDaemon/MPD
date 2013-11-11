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
#include "PcmDither.hxx"
#include "PcmBuffer.hxx"
#include "PcmUtils.hxx"

#include <type_traits>

template<SampleFormat F>
struct SampleTraits {};

template<>
struct SampleTraits<SampleFormat::S8> {
	typedef int8_t value_type;
	typedef value_type *pointer_type;
	typedef const value_type *const_pointer_type;

	static constexpr size_t SAMPLE_SIZE = sizeof(value_type);
	static constexpr unsigned BITS = sizeof(value_type) * 8;
};

template<>
struct SampleTraits<SampleFormat::S16> {
	typedef int16_t value_type;
	typedef value_type *pointer_type;
	typedef const value_type *const_pointer_type;

	static constexpr size_t SAMPLE_SIZE = sizeof(value_type);
	static constexpr unsigned BITS = sizeof(value_type) * 8;
};

template<>
struct SampleTraits<SampleFormat::S32> {
	typedef int32_t value_type;
	typedef value_type *pointer_type;
	typedef const value_type *const_pointer_type;

	static constexpr size_t SAMPLE_SIZE = sizeof(value_type);
	static constexpr unsigned BITS = sizeof(value_type) * 8;
};

template<>
struct SampleTraits<SampleFormat::S24_P32> {
	typedef int32_t value_type;
	typedef value_type *pointer_type;
	typedef const value_type *const_pointer_type;

	static constexpr size_t SAMPLE_SIZE = sizeof(value_type);
	static constexpr unsigned BITS = 24;
};

static void
pcm_convert_8_to_16(int16_t *out, const int8_t *in, const int8_t *in_end)
{
	while (in < in_end) {
		*out++ = *in++ << 8;
	}
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
		 const float *src, const float *end)
{
	constexpr auto bits = Traits::BITS;

	const float factor = 1 << (bits - 1);

	while (src != end) {
		int sample(*src++ * factor);
		*dest++ = PcmClamp<typename Traits::value_type, int, bits>(sample);
	}
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static void
ConvertFromFloat(typename Traits::pointer_type dest,
		 const float *src, size_t size)
{
	ConvertFromFloat<F, Traits>(dest, src,
				    pcm_end_pointer(src, size));
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
	ConvertFromFloat<F, Traits>(dest, src, src_size);
	return dest;
}

static int16_t *
pcm_allocate_8_to_16(PcmBuffer &buffer,
		     const int8_t *src, size_t src_size, size_t *dest_size_r)
{
	int16_t *dest;
	*dest_size_r = src_size / sizeof(*src) * sizeof(*dest);
	dest = (int16_t *)buffer.Get(*dest_size_r);
	pcm_convert_8_to_16(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int16_t *
pcm_allocate_24p32_to_16(PcmBuffer &buffer, PcmDither &dither,
			 const int32_t *src, size_t src_size,
			 size_t *dest_size_r)
{
	int16_t *dest;
	*dest_size_r = src_size / 2;
	assert(*dest_size_r == src_size / sizeof(*src) * sizeof(*dest));
	dest = (int16_t *)buffer.Get(*dest_size_r);
	pcm_convert_24_to_16(dither, dest, src,
			     pcm_end_pointer(src, src_size));
	return dest;
}

static int16_t *
pcm_allocate_32_to_16(PcmBuffer &buffer, PcmDither &dither,
		      const int32_t *src, size_t src_size,
		      size_t *dest_size_r)
{
	int16_t *dest;
	*dest_size_r = src_size / 2;
	assert(*dest_size_r == src_size / sizeof(*src) * sizeof(*dest));
	dest = (int16_t *)buffer.Get(*dest_size_r);
	pcm_convert_32_to_16(dither, dest, src,
			     pcm_end_pointer(src, src_size));
	return dest;
}

static int16_t *
pcm_allocate_float_to_16(PcmBuffer &buffer,
			 const float *src, size_t src_size,
			 size_t *dest_size_r)
{
	return AllocateFromFloat<SampleFormat::S16>(buffer, src, src_size,
						    dest_size_r);
}

const int16_t *
pcm_convert_to_16(PcmBuffer &buffer, PcmDither &dither,
		  SampleFormat src_format, const void *src,
		  size_t src_size, size_t *dest_size_r)
{
	assert(src_size % sample_format_size(src_format) == 0);

	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_16(buffer,
					    (const int8_t *)src, src_size,
					    dest_size_r);

	case SampleFormat::S16:
		*dest_size_r = src_size;
		return (const int16_t *)src;

	case SampleFormat::S24_P32:
		return pcm_allocate_24p32_to_16(buffer, dither,
						(const int32_t *)src, src_size,
						dest_size_r);

	case SampleFormat::S32:
		return pcm_allocate_32_to_16(buffer, dither,
					     (const int32_t *)src, src_size,
					     dest_size_r);

	case SampleFormat::FLOAT:
		return pcm_allocate_float_to_16(buffer,
						(const float *)src, src_size,
						dest_size_r);
	}

	return nullptr;
}

static void
pcm_convert_8_to_24(int32_t *out, const int8_t *in, const int8_t *in_end)
{
	while (in < in_end)
		*out++ = *in++ << 16;
}

static void
pcm_convert_16_to_24(int32_t *out, const int16_t *in, const int16_t *in_end)
{
	while (in < in_end)
		*out++ = *in++ << 8;
}

static void
pcm_convert_32_to_24(int32_t *gcc_restrict out,
		     const int32_t *gcc_restrict in,
		     const int32_t *gcc_restrict in_end)
{
	while (in < in_end)
		*out++ = *in++ >> 8;
}

static int32_t *
pcm_allocate_8_to_24(PcmBuffer &buffer,
		     const int8_t *src, size_t src_size, size_t *dest_size_r)
{
	int32_t *dest;
	*dest_size_r = src_size / sizeof(*src) * sizeof(*dest);
	dest = (int32_t *)buffer.Get(*dest_size_r);
	pcm_convert_8_to_24(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_16_to_24(PcmBuffer &buffer,
		      const int16_t *src, size_t src_size, size_t *dest_size_r)
{
	int32_t *dest;
	*dest_size_r = src_size * 2;
	assert(*dest_size_r == src_size / sizeof(*src) * sizeof(*dest));
	dest = (int32_t *)buffer.Get(*dest_size_r);
	pcm_convert_16_to_24(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_32_to_24(PcmBuffer &buffer,
		      const int32_t *src, size_t src_size, size_t *dest_size_r)
{
	*dest_size_r = src_size;
	int32_t *dest = (int32_t *)buffer.Get(*dest_size_r);
	pcm_convert_32_to_24(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_float_to_24(PcmBuffer &buffer,
			 const float *src, size_t src_size,
			 size_t *dest_size_r)
{
	return AllocateFromFloat<SampleFormat::S24_P32>(buffer, src, src_size,
							dest_size_r);
}

const int32_t *
pcm_convert_to_24(PcmBuffer &buffer,
		  SampleFormat src_format, const void *src,
		  size_t src_size, size_t *dest_size_r)
{
	assert(src_size % sample_format_size(src_format) == 0);

	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_24(buffer,
					    (const int8_t *)src, src_size,
					    dest_size_r);

	case SampleFormat::S16:
		return pcm_allocate_16_to_24(buffer,
					     (const int16_t *)src, src_size,
					     dest_size_r);

	case SampleFormat::S24_P32:
		*dest_size_r = src_size;
		return (const int32_t *)src;

	case SampleFormat::S32:
		return pcm_allocate_32_to_24(buffer,
					     (const int32_t *)src, src_size,
					     dest_size_r);

	case SampleFormat::FLOAT:
		return pcm_allocate_float_to_24(buffer,
						(const float *)src, src_size,
						dest_size_r);
	}

	return nullptr;
}

static void
pcm_convert_8_to_32(int32_t *out, const int8_t *in, const int8_t *in_end)
{
	while (in < in_end)
		*out++ = *in++ << 24;
}

static void
pcm_convert_16_to_32(int32_t *out, const int16_t *in, const int16_t *in_end)
{
	while (in < in_end)
		*out++ = *in++ << 16;
}

static void
pcm_convert_24_to_32(int32_t *gcc_restrict out,
		     const int32_t *gcc_restrict in,
		     const int32_t *gcc_restrict in_end)
{
	while (in < in_end)
		*out++ = *in++ << 8;
}

static int32_t *
pcm_allocate_8_to_32(PcmBuffer &buffer,
		     const int8_t *src, size_t src_size, size_t *dest_size_r)
{
	int32_t *dest;
	*dest_size_r = src_size / sizeof(*src) * sizeof(*dest);
	dest = (int32_t *)buffer.Get(*dest_size_r);
	pcm_convert_8_to_32(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_16_to_32(PcmBuffer &buffer,
		      const int16_t *src, size_t src_size, size_t *dest_size_r)
{
	int32_t *dest;
	*dest_size_r = src_size * 2;
	assert(*dest_size_r == src_size / sizeof(*src) * sizeof(*dest));
	dest = (int32_t *)buffer.Get(*dest_size_r);
	pcm_convert_16_to_32(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_24p32_to_32(PcmBuffer &buffer,
			 const int32_t *src, size_t src_size,
			 size_t *dest_size_r)
{
	*dest_size_r = src_size;
	int32_t *dest = (int32_t *)buffer.Get(*dest_size_r);
	pcm_convert_24_to_32(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_float_to_32(PcmBuffer &buffer,
			 const float *src, size_t src_size,
			 size_t *dest_size_r)
{
	/* convert to S24_P32 first */
	int32_t *dest = pcm_allocate_float_to_24(buffer, src, src_size,
						 dest_size_r);

	/* convert to 32 bit in-place */
	pcm_convert_24_to_32(dest, dest, pcm_end_pointer(dest, *dest_size_r));
	return dest;
}

const int32_t *
pcm_convert_to_32(PcmBuffer &buffer,
		  SampleFormat src_format, const void *src,
		  size_t src_size, size_t *dest_size_r)
{
	assert(src_size % sample_format_size(src_format) == 0);

	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_32(buffer,
					    (const int8_t *)src, src_size,
					    dest_size_r);

	case SampleFormat::S16:
		return pcm_allocate_16_to_32(buffer,
					     (const int16_t *)src, src_size,
					     dest_size_r);

	case SampleFormat::S24_P32:
		return pcm_allocate_24p32_to_32(buffer,
						(const int32_t *)src, src_size,
						dest_size_r);

	case SampleFormat::S32:
		*dest_size_r = src_size;
		return (const int32_t *)src;

	case SampleFormat::FLOAT:
		return pcm_allocate_float_to_32(buffer,
						(const float *)src, src_size,
						dest_size_r);
	}

	return nullptr;
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static void
ConvertToFloat(float *dest,
	       typename Traits::const_pointer_type src,
	       typename Traits::const_pointer_type end)
{
	constexpr float factor = 0.5 / (1 << (Traits::BITS - 2));
	while (src != end)
		*dest++ = float(*src++) * factor;
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static void
ConvertToFloat(float *dest,
	       typename Traits::const_pointer_type src, size_t size)
{
	ConvertToFloat<F, Traits>(dest, src, pcm_end_pointer(src, size));
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static float *
AllocateToFloat(PcmBuffer &buffer,
		typename Traits::const_pointer_type src, size_t src_size,
		size_t *dest_size_r)
{
	constexpr size_t src_sample_size = Traits::SAMPLE_SIZE;
	assert(src_size % src_sample_size == 0);

	const size_t num_samples = src_size / src_sample_size;
	*dest_size_r = num_samples * sizeof(float);
	float *dest = (float *)buffer.Get(*dest_size_r);
	ConvertToFloat<F, Traits>(dest, src, src_size);
	return dest;
}

static float *
pcm_allocate_8_to_float(PcmBuffer &buffer,
			const int8_t *src, size_t src_size,
			size_t *dest_size_r)
{
	return AllocateToFloat<SampleFormat::S8>(buffer, src, src_size,
						 dest_size_r);
}

static float *
pcm_allocate_16_to_float(PcmBuffer &buffer,
			 const int16_t *src, size_t src_size,
			 size_t *dest_size_r)
{
	return AllocateToFloat<SampleFormat::S16>(buffer, src, src_size,
						  dest_size_r);
}

static float *
pcm_allocate_24p32_to_float(PcmBuffer &buffer,
			    const int32_t *src, size_t src_size,
			    size_t *dest_size_r)
{
	return AllocateToFloat<SampleFormat::S24_P32>(buffer, src, src_size,
						      dest_size_r);
}

static float *
pcm_allocate_32_to_float(PcmBuffer &buffer,
			 const int32_t *src, size_t src_size,
			 size_t *dest_size_r)
{
	return AllocateToFloat<SampleFormat::S32>(buffer, src, src_size,
						  dest_size_r);
}

const float *
pcm_convert_to_float(PcmBuffer &buffer,
		     SampleFormat src_format, const void *src,
		     size_t src_size, size_t *dest_size_r)
{
	switch (src_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		break;

	case SampleFormat::S8:
		return pcm_allocate_8_to_float(buffer,
					       (const int8_t *)src, src_size,
					       dest_size_r);

	case SampleFormat::S16:
		return pcm_allocate_16_to_float(buffer,
						(const int16_t *)src, src_size,
						dest_size_r);

	case SampleFormat::S24_P32:
		return pcm_allocate_24p32_to_float(buffer,
						   (const int32_t *)src, src_size,
						   dest_size_r);

	case SampleFormat::S32:
		return pcm_allocate_32_to_float(buffer,
						(const int32_t *)src, src_size,
						dest_size_r);

	case SampleFormat::FLOAT:
		*dest_size_r = src_size;
		return (const float *)src;
	}

	return nullptr;
}
