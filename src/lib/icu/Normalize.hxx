// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <unicode/umachine.h>

#include <string_view>

template<class T> class AllocatedArray;

/**
 * @return the normalized string (or nullptr on error)
 */
[[gnu::pure]]
AllocatedArray<UChar>
IcuNormalize(std::basic_string_view<UChar> src) noexcept;

/**
 * @return the normalized string (or nullptr on error)
 */
[[gnu::pure]]
AllocatedArray<UChar>
IcuNormalizeCaseFold(std::basic_string_view<UChar> src) noexcept;
