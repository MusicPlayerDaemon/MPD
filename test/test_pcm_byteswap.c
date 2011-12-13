/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "test_pcm_all.h"
#include "pcm_byteswap.h"
#include "pcm_buffer.h"
#include "test_glib_compat.h"

#include <glib.h>

void
test_pcm_byteswap_16(void)
{
	enum { N = 256 };
	int16_t src[N];

	for (unsigned i = 0; i < G_N_ELEMENTS(src); ++i)
		src[i] = g_random_int();

	struct pcm_buffer buffer;
	pcm_buffer_init(&buffer);

	const int16_t *dest = pcm_byteswap_16(&buffer, src, sizeof(src));
	g_assert(dest != NULL);
	for (unsigned i = 0; i < N; ++i)
		g_assert_cmpint(dest[i], ==,
				(int16_t)GUINT16_SWAP_LE_BE(src[i]));
}

void
test_pcm_byteswap_32(void)
{
	enum { N = 256 };
	int32_t src[N];

	for (unsigned i = 0; i < G_N_ELEMENTS(src); ++i)
		src[i] = g_random_int();

	struct pcm_buffer buffer;
	pcm_buffer_init(&buffer);

	const int32_t *dest = pcm_byteswap_32(&buffer, src, sizeof(src));
	g_assert(dest != NULL);
	for (unsigned i = 0; i < N; ++i)
		g_assert_cmpint(dest[i], ==,
				(int32_t)GUINT32_SWAP_LE_BE(src[i]));
}
