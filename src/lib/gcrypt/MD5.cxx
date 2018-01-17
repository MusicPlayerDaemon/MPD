/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "MD5.hxx"
#include "util/ConstBuffer.hxx"

#include <gcrypt.h>

#include <stdio.h>

std::array<uint8_t, 16>
MD5(ConstBuffer<void> input) noexcept
{
	std::array<uint8_t, 16> result;
	gcry_md_hash_buffer(GCRY_MD_MD5, &result.front(),
			    input.data, input.size);
	return result;
}

std::array<char, 33>
MD5Hex(ConstBuffer<void> input) noexcept
{
	const auto raw = MD5(input);
	std::array<char, 33> result;

	char *p = &result.front();
	for (const auto i : raw) {
		sprintf(p, "%02x", i);
		p += 2;
	}

	return result;
}
