// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
