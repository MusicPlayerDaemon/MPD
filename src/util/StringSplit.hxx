// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <algorithm>
#include <string_view>
#include <type_traits>

template<typename T>
constexpr std::pair<std::basic_string_view<T>, std::basic_string_view<T>>
Partition(const std::basic_string_view<T> haystack,
	  const typename std::basic_string_view<T>::size_type position) noexcept
{
	return {
		haystack.substr(0, position),
		haystack.substr(position),
	};
}

template<typename T>
constexpr std::pair<std::basic_string_view<T>, std::basic_string_view<T>>
Partition(const std::basic_string_view<T> haystack,
	  const typename std::basic_string_view<T>::const_pointer position) noexcept
{
	return Partition(haystack, position - haystack.data());
}

template<typename T>
requires(!std::is_same_v<typename std::basic_string_view<T>::const_pointer,
	 typename std::basic_string_view<T>::const_iterator>)
constexpr std::pair<std::basic_string_view<T>, std::basic_string_view<T>>
Partition(const std::basic_string_view<T> haystack,
	  const typename std::basic_string_view<T>::const_iterator i) noexcept
{
	return Partition(haystack, i - haystack.begin());
}

template<typename T>
constexpr std::pair<std::basic_string_view<T>, std::basic_string_view<T>>
PartitionWithout(const std::basic_string_view<T> haystack,
		 const typename std::basic_string_view<T>::size_type separator) noexcept
{
	return {
		haystack.substr(0, separator),
		haystack.substr(separator + 1),
	};
}

/**
 * Split the string at the first occurrence of the given character.
 * If the character is not found, then the first value is the whole
 * string and the second value is nullptr.
 */
template<typename T>
constexpr std::pair<std::basic_string_view<T>, std::basic_string_view<T>>
Split(const std::basic_string_view<T> haystack, const T ch) noexcept
{
	const auto i = haystack.find(ch);
	if (i == haystack.npos)
		return {haystack, {}};

	return PartitionWithout(haystack, i);
}

/**
 * Split the string at the last occurrence of the given
 * character.  If the character is not found, then the first
 * value is the whole string and the second value is nullptr.
 */
template<typename T>
constexpr std::pair<std::basic_string_view<T>, std::basic_string_view<T>>
SplitLast(const std::basic_string_view<T> haystack, const T ch) noexcept
{
	const auto i = haystack.rfind(ch);
	if (i == haystack.npos)
		return {haystack, {}};

	return PartitionWithout(haystack, i);
}

/**
 * Find the first character that does not match the given predicate
 * and split at this boundary.
 */
template<typename T, typename P>
constexpr std::pair<std::basic_string_view<T>, std::basic_string_view<T>>
SplitWhile(const std::basic_string_view<T> haystack, P &&predicate) noexcept
{
	const auto i = std::find_if_not(haystack.begin(), haystack.end(),
					std::forward<P>(predicate));
	return Partition(haystack, i);
}
