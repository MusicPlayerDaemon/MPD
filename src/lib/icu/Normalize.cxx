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

#include "Normalize.hxx"
#include "util/AllocatedArray.hxx"

#include <unicode/unorm2.h>

static AllocatedArray<UChar>
IcuNormalize(const UNormalizer2 *norm2, std::basic_string_view<UChar> src) noexcept
{
	AllocatedArray<UChar> dest{src.size() * 2U};

	UErrorCode error_code = U_ZERO_ERROR;
	auto dest_length = unorm2_normalize(norm2, src.data(), src.size(),
					    dest.data(), dest.size(),
					    &error_code);
	if (U_FAILURE(error_code))
		return nullptr;

	dest.SetSize(dest_length);
	return dest;
}

AllocatedArray<UChar>
IcuNormalize(std::basic_string_view<UChar> src) noexcept
{
	UErrorCode error_code = U_ZERO_ERROR;
	auto *norm2 = unorm2_getNFKCInstance(&error_code);
	if (U_FAILURE(error_code))
		return nullptr;

	return IcuNormalize(norm2, src);
}

AllocatedArray<UChar>
IcuNormalizeCaseFold(std::basic_string_view<UChar> src) noexcept
{
	UErrorCode error_code = U_ZERO_ERROR;
	auto *norm2 = unorm2_getNFKCCasefoldInstance(&error_code);
	if (U_FAILURE(error_code))
		return nullptr;

	return IcuNormalize(norm2, src);
}
