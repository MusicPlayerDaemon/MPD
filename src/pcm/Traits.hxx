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

#ifndef MPD_PCM_TRAITS_HXX
#define MPD_PCM_TRAITS_HXX

#include "check.h"
#include "AudioFormat.hxx"

#include <stdint.h>

/**
 * This template describes the specified #SampleFormat.  This is an
 * empty prototype; the specializations contain the real definitions.
 * See SampleTraits<uint8_t> for more documentation.
 */
template<SampleFormat F>
struct SampleTraits {};

template<>
struct SampleTraits<SampleFormat::S8> {
	/**
	 * The type used for one sample value.
	 */
	typedef int8_t value_type;

	/**
	 * A writable pointer.
	 */
	typedef value_type *pointer_type;

	/**
	 * A read-only pointer.
	 */
	typedef const value_type *const_pointer_type;

	/**
	 * The size of one sample in bytes.
	 */
	static constexpr size_t SAMPLE_SIZE = sizeof(value_type);

	/**
	 * The integer bit depth of one sample.  This attribute may
	 * not exist if this is not an integer sample format.
	 */
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

#endif
