/*
 * Copyright 2003-2022 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
