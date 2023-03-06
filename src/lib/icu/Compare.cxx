// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Compare.hxx"
#include "Canonicalize.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "config.h"

#ifdef _WIN32
#include "Win32.hxx"
#include <windows.h>
#endif

#ifdef HAVE_ICU_CANONICALIZE

IcuCompare::IcuCompare(std::string_view _needle) noexcept
	:needle(IcuCanonicalize(_needle, true)) {}

#elif defined(_WIN32)

IcuCompare::IcuCompare(std::string_view _needle) noexcept
	:needle(nullptr)
{
	try {
		needle = MultiByteToWideChar(CP_UTF8, _needle);
	} catch (...) {
	}
}

#else

IcuCompare::IcuCompare(std::string_view _needle) noexcept
	:needle(_needle) {}

#endif

bool
IcuCompare::operator==(const char *haystack) const noexcept
{
#ifdef HAVE_ICU_CANONICALIZE
	return StringIsEqual(IcuCanonicalize(haystack, true).c_str(), needle.c_str());
#elif defined(_WIN32)
	if (needle == nullptr)
		/* the MultiByteToWideChar() call in the constructor
		   has failed, so let's always fail the comparison */
		return false;

	try {
		auto w_haystack = MultiByteToWideChar(CP_UTF8, haystack);
		return CompareStringEx(LOCALE_NAME_INVARIANT,
				       NORM_IGNORECASE,
				       w_haystack.c_str(), -1,
				       needle.c_str(), -1,
				       nullptr, nullptr, 0) == CSTR_EQUAL;
	} catch (...) {
		return false;
	}
#else
	return StringIsEqualIgnoreCase(haystack, needle.c_str());
#endif
}

bool
IcuCompare::IsIn(const char *haystack) const noexcept
{
#ifdef HAVE_ICU_CANONICALIZE
	return StringFind(IcuCanonicalize(haystack, true).c_str(),
			  needle.c_str()) != nullptr;
#elif defined(_WIN32)
	if (needle == nullptr)
		/* the MultiByteToWideChar() call in the constructor
		   has failed, so let's always fail the comparison */
		return false;

	try {
		auto w_haystack = MultiByteToWideChar(CP_UTF8, haystack);
		return FindNLSStringEx(LOCALE_NAME_INVARIANT,
				       FIND_FROMSTART|NORM_IGNORECASE,
				       w_haystack.c_str(), -1,
				       needle.c_str(), -1,
				       nullptr,
				       nullptr, nullptr, 0) >= 0;
	} catch (...) {
		/* MultiByteToWideChar() has failed */
		return false;
	}
#elif defined(HAVE_STRCASESTR)
	return strcasestr(haystack, needle.c_str()) != nullptr;
#else
	/* poor man's strcasestr() */
	for (const size_t length = strlen(needle.c_str());
	     *haystack != 0; ++haystack)
		if (StringIsEqualIgnoreCase(haystack, needle.c_str(), length))
			return true;

	return false;
#endif
}

bool
IcuCompare::StartsWith(const char *haystack) const noexcept
{
#ifdef HAVE_ICU_CANONICALIZE
	return StringStartsWith(IcuCanonicalize(haystack, true).c_str(),
				needle);
#elif defined(_WIN32)
	if (needle == nullptr)
		/* the MultiByteToWideChar() call in the constructor
		   has failed, so let's always fail the comparison */
		return false;

	try {
		auto w_haystack = MultiByteToWideChar(CP_UTF8, haystack);
		return FindNLSStringEx(LOCALE_NAME_INVARIANT,
				       FIND_STARTSWITH|NORM_IGNORECASE,
				       w_haystack.c_str(), -1,
				       needle.c_str(), -1,
				       nullptr,
				       nullptr, nullptr, 0) == 0;
	} catch (...) {
		/* MultiByteToWideChar() has failed */
		return false;
	}
#else
	return StringStartsWith(haystack, needle);
#endif
}
