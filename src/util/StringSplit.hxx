/*
 * Copyright 2013-2022 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <string_view>

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
