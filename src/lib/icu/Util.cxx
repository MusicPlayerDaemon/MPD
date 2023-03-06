// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Util.hxx"
#include "Error.hxx"
#include "util/AllocatedString.hxx"
#include "util/AllocatedArray.hxx"

#include <unicode/ustring.h>

#include <cassert>
#include <memory>

#include <string.h>

AllocatedArray<UChar>
UCharFromUTF8(std::string_view src)
{
	const size_t dest_capacity = src.size();
	AllocatedArray<UChar> dest(dest_capacity);

	UErrorCode error_code = U_ZERO_ERROR;
	int32_t dest_length;
	u_strFromUTF8(dest.data(), dest_capacity, &dest_length,
		      src.data(), src.size(),
		      &error_code);
	if (U_FAILURE(error_code))
		throw ICU::MakeError(error_code,
				     "Conversion from UTF-8 failed");

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
		throw ICU::MakeError(error_code,
				     "Conversion to UTF-8 failed");

	dest[dest_length] = 0;
	return AllocatedString::Donate(dest.release());
}
