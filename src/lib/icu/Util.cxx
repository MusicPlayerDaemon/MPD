/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Util.hxx"
#include "util/AllocatedString.hxx"
#include "util/AllocatedArray.hxx"
#include "util/ConstBuffer.hxx"

#include <unicode/ustring.h>

#include <cassert>
#include <memory>
#include <stdexcept>

#include <string.h>

AllocatedArray<UChar>
UCharFromUTF8(std::string_view src)
{
	const size_t dest_capacity = src.size();
	AllocatedArray<UChar> dest(dest_capacity);

	UErrorCode error_code = U_ZERO_ERROR;
	int32_t dest_length;
	u_strFromUTF8(dest.begin(), dest_capacity, &dest_length,
		      src.data(), src.size(),
		      &error_code);
	if (U_FAILURE(error_code))
		throw std::runtime_error(u_errorName(error_code));

	dest.SetSize(dest_length);
	return dest;
}

AllocatedString
UCharToUTF8(std::basic_string_view<UChar> src)
{
	/* worst-case estimate */
	size_t dest_capacity = 4 * src.size();

	auto dest = std::make_unique<char[]>(dest_capacity + 1);

	UErrorCode error_code = U_ZERO_ERROR;
	int32_t dest_length;
	u_strToUTF8(dest.get(), dest_capacity, &dest_length,
		    src.data(), src.size(),
		    &error_code);
	if (U_FAILURE(error_code))
		throw std::runtime_error(u_errorName(error_code));

	dest[dest_length] = 0;
	return AllocatedString::Donate(dest.release());
}
