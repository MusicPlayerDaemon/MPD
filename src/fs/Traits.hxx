/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_FS_TRAITS_HXX
#define MPD_FS_TRAITS_HXX

#include "util/Compiler.h"
#include "util/StringPointer.hxx"
#include "util/StringAPI.hxx"

#ifdef _WIN32
#include "util/CharUtil.hxx"
#include <tchar.h>
#endif

#include <cassert>
#include <string>

#ifdef _WIN32
#define PATH_LITERAL(s) _T(s)
#else
#define PATH_LITERAL(s) (s)
#endif

/**
 * This class describes the nature of a native filesystem path.
 */
struct PathTraitsFS {
#ifdef _WIN32
	using string = std::wstring;
	using string_view = std::wstring_view;
#else
	using string = std::string;
	using string_view = std::string_view;
#endif
	using char_traits = string::traits_type;
	using value_type = char_traits::char_type;
	using Pointer = StringPointer<value_type>;
	using pointer = Pointer::pointer;
	using const_pointer = Pointer::const_pointer;

#ifdef _WIN32
	static constexpr value_type SEPARATOR = '\\';
#else
	static constexpr value_type SEPARATOR = '/';
#endif

	static constexpr const_pointer CURRENT_DIRECTORY = PATH_LITERAL(".");

	static constexpr bool IsSeparator(value_type ch) noexcept {
		return
#ifdef _WIN32
			ch == '/' ||
#endif
			ch == SEPARATOR;
	}

	[[gnu::pure]] [[gnu::nonnull]]
	static const_pointer FindLastSeparator(const_pointer p) noexcept {
#if !CLANG_CHECK_VERSION(3,6)
		/* disabled on clang due to -Wtautological-pointer-compare */
		assert(p != nullptr);
#endif

#ifdef _WIN32
		const_pointer pos = p + GetLength(p);
		while (p != pos && !IsSeparator(*pos))
			--pos;
		return IsSeparator(*pos) ? pos : nullptr;
#else
		return StringFindLast(p, SEPARATOR);
#endif
	}

	[[gnu::pure]]
	static const_pointer FindLastSeparator(string_view p) noexcept {
#ifdef _WIN32
		const_pointer pos = p.data() + p.size();
		while (p.data() != pos && !IsSeparator(*pos))
			--pos;
		return IsSeparator(*pos) ? pos : nullptr;
#else
		return StringFindLast(p.data(), SEPARATOR, p.size());
#endif
	}

	[[gnu::pure]]
	static const_pointer GetFilenameSuffix(const_pointer filename) noexcept {
		const_pointer dot = StringFindLast(filename, '.');
		return dot != nullptr && dot > filename && dot[1] != 0
			? dot + 1
			: nullptr;
	}

	[[gnu::pure]]
	static const_pointer GetPathSuffix(const_pointer path) noexcept {
		return GetFilenameSuffix(GetBase(path));
	}

#ifdef _WIN32
	[[gnu::pure]] [[gnu::nonnull]]
	static constexpr bool IsDrive(const_pointer p) noexcept {
		return IsAlphaASCII(p[0]) && p[1] == ':';
	}

	static constexpr bool IsDrive(string_view p) noexcept {
		return p.size() >= 2 && IsAlphaASCII(p[0]) && p[1] == ':';
	}
#endif

	[[gnu::pure]] [[gnu::nonnull]]
	static bool IsAbsolute(const_pointer p) noexcept {
#if !CLANG_CHECK_VERSION(3,6)
		/* disabled on clang due to -Wtautological-pointer-compare */
		assert(p != nullptr);
#endif

#ifdef _WIN32
		if (IsDrive(p) && IsSeparator(p[2]))
			return true;
#endif
		return IsSeparator(*p);
	}

	[[gnu::pure]] [[gnu::nonnull]]
	static bool IsSpecialFilename(const_pointer name) noexcept {
		return (name[0] == '.' && name[1] == 0) ||
			(name[0] == '.' && name[1] == '.' && name[2] == 0);
	}

	[[gnu::pure]] [[gnu::nonnull]]
	static size_t GetLength(const_pointer p) noexcept {
		return StringLength(p);
	}

	[[gnu::pure]] [[gnu::nonnull]]
	static const_pointer Find(const_pointer p, value_type ch) noexcept {
		return StringFind(p, ch);
	}

	/**
	 * Determine the "base" file name of the given native path.
	 * The return value points inside the given string.
	 */
	[[gnu::pure]] [[gnu::nonnull]]
	static const_pointer GetBase(const_pointer p) noexcept;

	/**
	 * Determine the "parent" file name of the given native path.
	 * As a special case, returns the string "." if there is no
	 * separator in the given input string.
	 */
	[[gnu::pure]] [[gnu::nonnull]]
	static string_view GetParent(const_pointer p) noexcept;

	[[gnu::pure]]
	static string_view GetParent(string_view p) noexcept;

	/**
	 * Determine the relative part of the given path to this
	 * object, not including the directory separator.  Returns an
	 * empty string if the given path equals this object or
	 * nullptr on mismatch.
	 */
	[[gnu::pure]] [[gnu::nonnull]]
	static const_pointer Relative(string_view base, const_pointer other) noexcept;

	[[gnu::pure]]
	static string_view Relative(string_view base, string_view other) noexcept;

	/**
	 * Constructs the path from the given components.
	 * If either of the components is empty string,
	 * remaining component is returned unchanged.
	 * If both components are empty strings, empty string is returned.
	 */
	[[gnu::pure]]
	static string Build(string_view a, string_view b) noexcept;

	/**
	 * Interpret the given path as being relative to the given
	 * base, and return the concatenated path.
	 */
	[[gnu::pure]]
	static string Apply(const_pointer base,
			    const_pointer path) noexcept;
};

/**
 * This class describes the nature of a MPD internal filesystem path.
 */
struct PathTraitsUTF8 {
	using string = std::string;
	using string_view = std::string_view;
	using char_traits = string::traits_type;
	using value_type = char_traits::char_type;
	using pointer = value_type *;
	using const_pointer = const value_type *;

	static constexpr value_type SEPARATOR = '/';

	static constexpr const_pointer CURRENT_DIRECTORY = ".";

	static constexpr bool IsSeparator(value_type ch) {
		return ch == SEPARATOR;
	}

	[[gnu::pure]] [[gnu::nonnull]]
	static const_pointer FindLastSeparator(const_pointer p) noexcept {
#if !CLANG_CHECK_VERSION(3,6)
		/* disabled on clang due to -Wtautological-pointer-compare */
		assert(p != nullptr);
#endif

		return std::strrchr(p, SEPARATOR);
	}

	[[gnu::pure]]
	static const_pointer FindLastSeparator(string_view p) noexcept {
		return StringFindLast(p.data(), SEPARATOR, p.size());
	}

	[[gnu::pure]]
	static const_pointer GetFilenameSuffix(const_pointer filename) noexcept {
		const_pointer dot = StringFindLast(filename, '.');
		return dot != nullptr && dot > filename && dot[1] != 0
			? dot + 1
			: nullptr;
	}

	[[gnu::pure]]
	static const_pointer GetPathSuffix(const_pointer path) noexcept {
		return GetFilenameSuffix(GetBase(path));
	}

#ifdef _WIN32
	[[gnu::pure]] [[gnu::nonnull]]
	static constexpr bool IsDrive(const_pointer p) noexcept {
		return IsAlphaASCII(p[0]) && p[1] == ':';
	}

	static constexpr bool IsDrive(string_view p) noexcept {
		return p.size() >= 2 && IsAlphaASCII(p[0]) && p[1] == ':';
	}
#endif

	[[gnu::pure]] [[gnu::nonnull]]
	static bool IsAbsolute(const_pointer p) noexcept {
#if !CLANG_CHECK_VERSION(3,6)
		/* disabled on clang due to -Wtautological-pointer-compare */
		assert(p != nullptr);
#endif

#ifdef _WIN32
		if (IsDrive(p) && IsSeparator(p[2]))
			return true;
#endif
		return IsSeparator(*p);
	}

	/**
	 * Is this any kind of absolute URI? (Unlike IsAbsolute(),
	 * this includes URIs/URLs with a scheme)
	 */
	[[gnu::pure]] [[gnu::nonnull]]
	static bool IsAbsoluteOrHasScheme(const_pointer p) noexcept;

	[[gnu::pure]] [[gnu::nonnull]]
	static bool IsSpecialFilename(const_pointer name) noexcept {
		return (name[0] == '.' && name[1] == 0) ||
			(name[0] == '.' && name[1] == '.' && name[2] == 0);
	}

	[[gnu::pure]] [[gnu::nonnull]]
	static size_t GetLength(const_pointer p) noexcept {
		return StringLength(p);
	}

	[[gnu::pure]] [[gnu::nonnull]]
	static const_pointer Find(const_pointer p, value_type ch) noexcept {
		return StringFind(p, ch);
	}

	/**
	 * Determine the "base" file name of the given UTF-8 path.
	 * The return value points inside the given string.
	 */
	[[gnu::pure]] [[gnu::nonnull]]
	static const_pointer GetBase(const_pointer p) noexcept;

	/**
	 * Determine the "parent" file name of the given UTF-8 path.
	 * As a special case, returns the string "." if there is no
	 * separator in the given input string.
	 */
	[[gnu::pure]] [[gnu::nonnull]]
	static string_view GetParent(const_pointer p) noexcept;

	[[gnu::pure]]
	static string_view GetParent(string_view p) noexcept;

	/**
	 * Determine the relative part of the given path to this
	 * object, not including the directory separator.  Returns an
	 * empty string if the given path equals this object or
	 * nullptr on mismatch.
	 */
	[[gnu::pure]] [[gnu::nonnull]]
	static const_pointer Relative(string_view base, const_pointer other) noexcept;

	[[gnu::pure]]
	static string_view Relative(string_view base, string_view other) noexcept;

	/**
	 * Constructs the path from the given components.
	 * If either of the components is empty string,
	 * remaining component is returned unchanged.
	 * If both components are empty strings, empty string is returned.
	 */
	[[gnu::pure]]
	static string Build(string_view a, string_view b) noexcept;
};

#endif
