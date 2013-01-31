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
#include "test_pcm_all.hxx"
#include "test_pcm_util.hxx"
#include "PcmFormat.hxx"
#include "PcmDither.hxx"
#include "PcmUtils.hxx"
#include "pcm_buffer.h"
#include "audio_format.h"

#include <glib.h>

void
test_pcm_format_8_to_16()
{
	constexpr unsigned N = 256;
	const auto src = TestDataBuffer<int8_t, N>();

	struct pcm_buffer buffer;
	pcm_buffer_init(&buffer);

	size_t d_size;
	PcmDither dither;
	auto d = pcm_convert_to_16(&buffer, dither, SAMPLE_FORMAT_S8,
				   src, sizeof(src), &d_size);
	auto d_end = pcm_end_pointer(d, d_size);
	g_assert_cmpint(d_end - d, ==, N);

	for (size_t i = 0; i < N; ++i)
		g_assert_cmpint(src[i], ==, d[i] >> 8);

	pcm_buffer_deinit(&buffer);
}

void
test_pcm_format_16_to_24()
{
	constexpr unsigned N = 256;
	const auto src = TestDataBuffer<int16_t, N>();

	struct pcm_buffer buffer;
	pcm_buffer_init(&buffer);

	size_t d_size;
	auto d = pcm_convert_to_24(&buffer, SAMPLE_FORMAT_S16,
				   src, sizeof(src), &d_size);
	auto d_end = pcm_end_pointer(d, d_size);
	g_assert_cmpint(d_end - d, ==, N);

	for (size_t i = 0; i < N; ++i)
		g_assert_cmpint(src[i], ==, d[i] >> 8);

	pcm_buffer_deinit(&buffer);
}

void
test_pcm_format_16_to_32()
{
	constexpr unsigned N = 256;
	const auto src = TestDataBuffer<int16_t, N>();

	struct pcm_buffer buffer;
	pcm_buffer_init(&buffer);

	size_t d_size;
	auto d = pcm_convert_to_32(&buffer, SAMPLE_FORMAT_S16,
				   src, sizeof(src), &d_size);
	auto d_end = pcm_end_pointer(d, d_size);
	g_assert_cmpint(d_end - d, ==, N);

	for (size_t i = 0; i < N; ++i)
		g_assert_cmpint(src[i], ==, d[i] >> 16);

	pcm_buffer_deinit(&buffer);
}

void
test_pcm_format_float()
{
	constexpr unsigned N = 256;
	const auto src = TestDataBuffer<int16_t, N>();

	struct pcm_buffer buffer1, buffer2;
	pcm_buffer_init(&buffer1);
	pcm_buffer_init(&buffer2);

	size_t f_size;
	auto f = pcm_convert_to_float(&buffer1, SAMPLE_FORMAT_S16,
				      src, sizeof(src), &f_size);
	auto f_end = pcm_end_pointer(f, f_size);
	g_assert_cmpint(f_end - f, ==, N);

	for (auto i = f; i != f_end; ++i) {
		g_assert(*i >= -1.);
		g_assert(*i <= 1.);
	}

	PcmDither dither;

	size_t d_size;
	auto d = pcm_convert_to_16(&buffer2, dither,
				   SAMPLE_FORMAT_FLOAT,
				   f, f_size, &d_size);
	auto d_end = pcm_end_pointer(d, d_size);
	g_assert_cmpint(d_end - d, ==, N);

	for (size_t i = 0; i < N; ++i)
		g_assert_cmpint(src[i], ==, d[i]);

	pcm_buffer_deinit(&buffer1);
	pcm_buffer_deinit(&buffer2);
}
