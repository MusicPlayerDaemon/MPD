// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
