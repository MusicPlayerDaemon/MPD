// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef STRING_COMPARE_HXX
#define STRING_COMPARE_HXX

#include "StringAPI.hxx"

#ifdef _UNICODE
#include "WStringCompare.hxx"
#endif

#include <string_view>

[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringIsEmpty(const char *string) noexcept
{
	return *string == 0;
}

[[gnu::pure]]
static inline bool
StringIsEqual(std::string_view a, std::string_view b) noexcept
{
	return a.size() == b.size() &&
		StringIsEqual(a.data(), b.data(), b.size());
}

[[gnu::pure]]
static inline bool
StringIsEqualIgnoreCase(std::string_view a, std::string_view b) noexcept
{
	return a.size() == b.size() &&
		StringIsEqualIgnoreCase(a.data(), b.data(), b.size());
}

[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringStartsWith(const char *haystack, std::string_view needle) noexcept
{
	return StringIsEqual(haystack, needle.data(), needle.size());
}

[[gnu::pure]] [[gnu::nonnull]]
bool
StringEndsWith(const char *haystack, const char *needle) noexcept;

[[gnu::pure]] [[gnu::nonnull]]
bool
StringEndsWithIgnoreCase(const char *haystack, const char *needle) noexcept;

/**
 * Returns the portion of the string after a prefix.  If the string
 * does not begin with the specified prefix, this function returns
 * nullptr.
 */
[[gnu::pure]] [[gnu::nonnull]]
static inline const char *
StringAfterPrefix(const char *haystack, std::string_view needle) noexcept
{
	return StringStartsWith(haystack, needle)
		? haystack + needle.size()
		: nullptr;
}

[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringStartsWithIgnoreCase(const char *haystack, std::string_view needle) noexcept
{
	return StringIsEqualIgnoreCase(haystack, needle.data(), needle.size());
}

[[gnu::pure]]
static inline bool
StringStartsWithIgnoreCase(std::string_view haystack, std::string_view needle) noexcept
{
	return haystack.size() >= needle.size() &&
		StringIsEqualIgnoreCase(haystack.data(),
					needle.data(), needle.size());
}

/**
 * Returns the portion of the string after a prefix.  If the string
 * does not begin with the specified prefix, this function returns
 * nullptr.
 * This function is case-independent.
 */
[[gnu::pure]] [[gnu::nonnull]]
static inline const char *
StringAfterPrefixIgnoreCase(const char *haystack, std::string_view needle) noexcept
{
	return StringStartsWithIgnoreCase(haystack, needle)
		? haystack + needle.size()
		: nullptr;
}

[[gnu::pure]]
static inline std::string_view
StringAfterPrefixIgnoreCase(std::string_view haystack,
			    std::string_view needle) noexcept
{
	return StringStartsWithIgnoreCase(haystack, needle)
		? haystack.substr(needle.size())
		: std::string_view{};
}

/**
 * Check if the given string ends with the specified suffix.  If yes,
 * returns the position of the suffix, and nullptr otherwise.
 */
[[gnu::pure]] [[gnu::nonnull]]
const char *
FindStringSuffix(const char *p, const char *suffix) noexcept;

template<typename T>
bool
SkipPrefix(std::basic_string_view<T> &haystack,
	   std::basic_string_view<T> needle) noexcept
{
	bool match = haystack.starts_with(needle);
	if (match)
		haystack.remove_prefix(needle.size());
	return match;
}

template<typename T>
bool
RemoveSuffix(std::basic_string_view<T> &haystack,
	     std::basic_string_view<T> needle) noexcept
{
	bool match = haystack.ends_with(needle);
	if (match)
		haystack.remove_suffix(needle.size());
	return match;
}

#endif
