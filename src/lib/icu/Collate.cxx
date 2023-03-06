// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Collate.hxx"
#include "util/AllocatedString.hxx"
#include "config.h"

#ifdef HAVE_ICU
#include "Error.hxx"
#include "Util.hxx"

#include <unicode/ucol.h>
#include <unicode/ustring.h>
#else
#include <algorithm>

#ifndef _WIN32
#include <string>
#endif

#endif

#ifdef _WIN32
#include "Win32.hxx"
#include "util/AllocatedString.hxx"
#include <windows.h>
#endif

#include <cassert>
#include <memory>
#include <stdexcept>

#include <string.h>

#ifdef HAVE_ICU
static UCollator *collator;
#endif

#ifdef HAVE_ICU

void
IcuCollateInit()
{
	assert(collator == nullptr);

	UErrorCode code = U_ZERO_ERROR;
	collator = ucol_open("", &code);
	if (collator == nullptr)
		throw ICU::MakeError(code, "ucol_open() failed");
}

void
IcuCollateFinish() noexcept
{
	assert(collator != nullptr);

	ucol_close(collator);
	collator = nullptr;
}

#endif

[[gnu::pure]]
int
IcuCollate(std::string_view a, std::string_view b) noexcept
{
#ifdef HAVE_ICU
	assert(collator != nullptr);

	UErrorCode code = U_ZERO_ERROR;
	return (int)ucol_strcollUTF8(collator, a.data(), a.size(),
				     b.data(), b.size(), &code);

#elif defined(_WIN32)
	BasicAllocatedString<wchar_t> wa, wb;

	try {
		wa = MultiByteToWideChar(CP_UTF8, a);
	} catch (...) {
		try {
			wb = MultiByteToWideChar(CP_UTF8, b);
			return -1;
		} catch (...) {
			return 0;
		}
	}

	try {
		wb = MultiByteToWideChar(CP_UTF8, b);
	} catch (...) {
		return 1;
	}

	auto result = CompareStringEx(LOCALE_NAME_INVARIANT,
				      NORM_IGNORECASE,
				      wa.c_str(), -1,
				      wb.c_str(), -1,
				      nullptr, nullptr, 0);
	if (result != 0)
		/* "To maintain the C runtime convention of comparing
		   strings, the value 2 can be subtracted from a
		   nonzero return value." */
		result -= 2;

	return result;
#else
	/* need to duplicate for the fallback because std::string_view
	   is not null-terminated */
	return strcoll(std::string(a).c_str(), std::string(b).c_str());
#endif
}
