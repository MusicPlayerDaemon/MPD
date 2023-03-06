// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <unicode/utrans.h>

#include <utility> // for std::exchange()
#include <string_view>

template<class T> class AllocatedArray;

/**
 * Wrapper for an ICU #UTransliterator instance.
 */
class IcuTransliterator {
	UTransliterator *transliterator = nullptr;

	IcuTransliterator(UTransliterator *_transliterator) noexcept
		:transliterator(_transliterator) {}

public:
	IcuTransliterator() noexcept = default;

	/**
	 * Throws on error.
	 */
	IcuTransliterator(std::basic_string_view<UChar> id,
			  std::basic_string_view<UChar> rules);

	~IcuTransliterator() noexcept {
		if (transliterator != nullptr)
			    utrans_close(transliterator);
	}

	IcuTransliterator(IcuTransliterator &&src) noexcept
		:transliterator(std::exchange(src.transliterator, nullptr)) {}

	IcuTransliterator &operator=(IcuTransliterator &&src) noexcept {
		using std::swap;
		swap(transliterator, src.transliterator);
		return *this;
	}

	/**
	 * @return the transliterated string (or nullptr on error)
	 */
	[[gnu::pure]]
	AllocatedArray<UChar> Transliterate(std::basic_string_view<UChar> src) noexcept;
};
