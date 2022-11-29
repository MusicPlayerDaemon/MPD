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

#include "Transliterator.hxx"
#include "Error.hxx"
#include "util/AllocatedArray.hxx"

#include <algorithm> // for std::copy()
#include <stdexcept>

static UTransliterator *
OpenTransliterator(std::basic_string_view<UChar> id,
		   std::basic_string_view<UChar> rules)
{
	UErrorCode error_code = U_ZERO_ERROR;

	UTransliterator *t = utrans_openU(id.data(), id.size(),
					  UTRANS_FORWARD,
					  rules.data(), rules.size(),
					  nullptr, &error_code);
	if (t == nullptr)
		throw ICU::MakeError(error_code, "utrans_openU() failed");

	return t;
}

IcuTransliterator::IcuTransliterator(std::basic_string_view<UChar> id,
				     std::basic_string_view<UChar> rules)
	:transliterator(OpenTransliterator(id, rules))
{
}

AllocatedArray<UChar>
IcuTransliterator::Transliterate(std::basic_string_view<UChar> src) noexcept
{
	AllocatedArray<UChar> dest(src.size() * 2U);
	std::copy(src.begin(), src.end(), dest.begin());

	int32_t length = src.size();
	int32_t limit = length;

	UErrorCode status = U_ZERO_ERROR;
	utrans_transUChars(transliterator,
			   dest.data(), &length, dest.size(),
			   0, &limit,
			   &status);
	if (U_FAILURE(status))
		return nullptr;

	dest.SetSize(length);
	return dest;
}
