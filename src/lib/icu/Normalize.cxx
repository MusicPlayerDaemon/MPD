// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
