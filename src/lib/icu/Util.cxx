/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "Util.hxx"
#include "util/WritableBuffer.hxx"
#include "util/ConstBuffer.hxx"

#include <unicode/ustring.h>

#include <assert.h>
#include <string.h>

WritableBuffer<UChar>
UCharFromUTF8(const char *src)
{
	assert(src != nullptr);

	const size_t src_length = strlen(src);
	const size_t dest_capacity = src_length;
	UChar *dest = new UChar[dest_capacity];

	UErrorCode error_code = U_ZERO_ERROR;
	int32_t dest_length;
	u_strFromUTF8(dest, dest_capacity, &dest_length,
		      src, src_length,
		      &error_code);
	if (U_FAILURE(error_code)) {
		delete[] dest;
		return nullptr;
	}

	return { dest, size_t(dest_length) };
}

WritableBuffer<char>
UCharToUTF8(ConstBuffer<UChar> src)
{
	assert(!src.IsNull());

	/* worst-case estimate */
	size_t dest_capacity = 4 * src.size;

	char *dest = new char[dest_capacity];

	UErrorCode error_code = U_ZERO_ERROR;
	int32_t dest_length;
	u_strToUTF8(dest, dest_capacity, &dest_length, src.data, src.size,
		    &error_code);
	if (U_FAILURE(error_code)) {
		delete[] dest;
		return nullptr;
	}

	return { dest, size_t(dest_length) };
}
