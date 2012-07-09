/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#include "util/byte_reverse.h"

#include <glib.h>

static void
test_byte_reverse_2(void)
{
	static const char src[] = "123456";
	static const char result[] = "214365";
	static uint8_t dest[G_N_ELEMENTS(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + G_N_ELEMENTS(src) - 1), 2);
	g_assert_cmpstr((const char *)dest, ==, result);
}

static void
test_byte_reverse_3(void)
{
	static const char src[] = "123456";
	static const char result[] = "321654";
	static uint8_t dest[G_N_ELEMENTS(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + G_N_ELEMENTS(src) - 1), 3);
	g_assert_cmpstr((const char *)dest, ==, result);
}

static void
test_byte_reverse_4(void)
{
	static const char src[] = "12345678";
	static const char result[] = "43218765";
	static uint8_t dest[G_N_ELEMENTS(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + G_N_ELEMENTS(src) - 1), 4);
	g_assert_cmpstr((const char *)dest, ==, result);
}

static void
test_byte_reverse_5(void)
{
	static const char src[] = "1234567890";
	static const char result[] = "5432109876";
	static uint8_t dest[G_N_ELEMENTS(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + G_N_ELEMENTS(src) - 1), 5);
	g_assert_cmpstr((const char *)dest, ==, result);
}

int
main(int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func("/util/byte_reverse/2", test_byte_reverse_2);
	g_test_add_func("/util/byte_reverse/3", test_byte_reverse_3);
	g_test_add_func("/util/byte_reverse/4", test_byte_reverse_4);
	g_test_add_func("/util/byte_reverse/5", test_byte_reverse_5);

	g_test_run();
}
