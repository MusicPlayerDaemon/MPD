/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "FoldCase.hxx"
#include "util/AllocatedArray.hxx"

#include <unicode/ucol.h>
#include <unicode/ustring.h>

AllocatedArray<UChar>
IcuFoldCase(std::basic_string_view<UChar> src) noexcept
{
	AllocatedArray<UChar> dest(src.size() * 2U);

	UErrorCode error_code = U_ZERO_ERROR;
	auto length = u_strFoldCase(dest.data(), dest.size(),
				    src.data(), src.size(),
				    U_FOLD_CASE_DEFAULT,
				    &error_code);
	if (U_FAILURE(error_code))
		return nullptr;

	dest.SetSize(length);
	return dest;
}
