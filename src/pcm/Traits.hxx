// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "SampleFormat.hxx"

#include <cstddef>
#include <cstdint>

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
	typedef value_type *pointer;

	/**
	 * A read-only pointer.
	 */
	typedef const value_type *const_pointer;

	/**
	 * A "long" type that is large and accurate enough for adding
	 * two values without risking an (integer) overflow or
	 * (floating point) precision loss.
	 */
	typedef int sum_type;

	/**
	 * A "long" type that is large and accurate enough for
	 * arithmetic without risking an (integer) overflow or
	 * (floating point) precision loss.
	 */
	typedef int_least32_t long_type;

	/**
	 * The size of one sample in bytes.
	 */
	static constexpr size_t SAMPLE_SIZE = sizeof(value_type);

	/**
	 * The integer bit depth of one sample.  This attribute may
	 * not exist if this is not an integer sample format.
	 */
	static constexpr unsigned BITS = sizeof(value_type) * 8;

	/**
	 * The minimum sample value.
	 */
	static constexpr value_type MIN = -(sum_type(1) << (BITS - 1));

	/**
	 * The maximum sample value.
	 */
	static constexpr value_type MAX = (sum_type(1) << (BITS - 1)) - 1;

	/**
	 * A value which represents "silence".
	 */
	static constexpr value_type SILENCE = 0;
};

template<>
struct SampleTraits<SampleFormat::S16> {
	typedef int16_t value_type;
	typedef value_type *pointer;
	typedef const value_type *const_pointer;

	typedef int_least32_t sum_type;
	typedef int_least32_t long_type;

	static constexpr size_t SAMPLE_SIZE = sizeof(value_type);
	static constexpr unsigned BITS = sizeof(value_type) * 8;

	static constexpr value_type MIN = -(sum_type(1) << (BITS - 1));
	static constexpr value_type MAX = (sum_type(1) << (BITS - 1)) - 1;
	static constexpr value_type SILENCE = 0;
};

template<>
struct SampleTraits<SampleFormat::S32> {
	typedef int32_t value_type;
	typedef value_type *pointer;
	typedef const value_type *const_pointer;

	typedef int_least64_t sum_type;
	typedef int_least64_t long_type;

	static constexpr size_t SAMPLE_SIZE = sizeof(value_type);
	static constexpr unsigned BITS = sizeof(value_type) * 8;

	static constexpr value_type MIN = -(sum_type(1) << (BITS - 1));
	static constexpr value_type MAX = (sum_type(1) << (BITS - 1)) - 1;
	static constexpr value_type SILENCE = 0;
};

template<>
struct SampleTraits<SampleFormat::S24_P32> {
	typedef int32_t value_type;
	typedef value_type *pointer;
	typedef const value_type *const_pointer;

	typedef int_least32_t sum_type;
	typedef int_least64_t long_type;

	static constexpr size_t SAMPLE_SIZE = sizeof(value_type);
	static constexpr unsigned BITS = 24;

	static constexpr value_type MIN = -(sum_type(1) << (BITS - 1));
	static constexpr value_type MAX = (sum_type(1) << (BITS - 1)) - 1;
	static constexpr value_type SILENCE = 0;
};

template<>
struct SampleTraits<SampleFormat::FLOAT> {
	typedef float value_type;
	typedef value_type *pointer;
	typedef const value_type *const_pointer;

	typedef float sum_type;
	typedef float long_type;

	static constexpr size_t SAMPLE_SIZE = sizeof(value_type);

	static constexpr value_type MIN = -1;
	static constexpr value_type MAX = 1;
	static constexpr value_type SILENCE = 0;
};

template<>
struct SampleTraits<SampleFormat::DSD> {
	typedef std::byte value_type;
	typedef value_type *pointer;
	typedef const value_type *const_pointer;

	static constexpr size_t SAMPLE_SIZE = sizeof(value_type);

	/* 0x69 = 01101001
	 * This pattern "on repeat" makes a low energy 352.8 kHz tone
	 * and a high energy 1.0584 MHz tone which should be filtered
	 * out completely by any playback system --> silence
	 */
	static constexpr value_type SILENCE{0x69};
};
