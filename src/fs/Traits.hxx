/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include <string>

#include <assert.h>

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
	typedef std::wstring string;
#else
	typedef std::string string;
#endif
	typedef string::traits_type char_traits;
	typedef char_traits::char_type value_type;
	typedef StringPointer<value_type> Pointer;
	typedef Pointer::pointer_type pointer_type;
	typedef Pointer::const_pointer_type const_pointer_type;

#ifdef _WIN32
	static constexpr value_type SEPARATOR = '\\';
#else
	static constexpr value_type SEPARATOR = '/';
#endif

	static constexpr const_pointer_type CURRENT_DIRECTORY = PATH_LITERAL(".");

	static constexpr bool IsSeparator(value_type ch) noexcept {
		return
#ifdef _WIN32
			ch == '/' ||
#endif
			ch == SEPARATOR;
	}

	gcc_pure gcc_nonnull_all
	static const_pointer_type FindLastSeparator(const_pointer_type p) noexcept {
#if !CLANG_CHECK_VERSION(3,6)
		/* disabled on clang due to -Wtautological-pointer-compare */
		assert(p != nullptr);
#endif

#ifdef _WIN32
		const_pointer_type pos = p + GetLength(p);
		while (p != pos && !IsSeparator(*pos))
			--pos;
		return IsSeparator(*pos) ? pos : nullptr;
#else
		return StringFindLast(p, SEPARATOR);
#endif
	}

#ifdef _WIN32
	gcc_pure gcc_nonnull_all
	static constexpr bool IsDrive(const_pointer_type p) noexcept {
		return IsAlphaASCII(p[0]) && p[1] == ':';
	}
#endif

	gcc_pure gcc_nonnull_all
	static bool IsAbsolute(const_pointer_type p) noexcept {
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

	gcc_pure gcc_nonnull_all
	static size_t GetLength(const_pointer_type p) noexcept {
		return StringLength(p);
	}

	gcc_pure gcc_nonnull_all
	static const_pointer_type Find(const_pointer_type p, value_type ch) noexcept {
		return StringFind(p, ch);
	}

	/**
	 * Determine the "base" file name of the given native path.
	 * The return value points inside the given string.
	 */
	gcc_pure gcc_nonnull_all
	static const_pointer_type GetBase(const_pointer_type p) noexcept;

	/**
	 * Determine the "parent" file name of the given native path.
	 * As a special case, returns the string "." if there is no
	 * separator in the given input string.
	 */
	gcc_pure gcc_nonnull_all
	static string GetParent(const_pointer_type p) noexcept;

	/**
	 * Determine the relative part of the given path to this
	 * object, not including the directory separator.  Returns an
	 * empty string if the given path equals this object or
	 * nullptr on mismatch.
	 */
	gcc_pure gcc_nonnull_all
	static const_pointer_type Relative(const_pointer_type base,
					   const_pointer_type other) noexcept;

	/**
	 * Constructs the path from the given components.
	 * If either of the components is empty string,
	 * remaining component is returned unchanged.
	 * If both components are empty strings, empty string is returned.
	 */
	gcc_pure gcc_nonnull_all
	static string Build(const_pointer_type a, size_t a_size,
			    const_pointer_type b, size_t b_size) noexcept;

	gcc_pure gcc_nonnull_all
	static string Build(const_pointer_type a, const_pointer_type b) noexcept {
		return Build(a, GetLength(a), b, GetLength(b));
	}

	/**
	 * Interpret the given path as being relative to the given
	 * base, and return the concatenated path.
	 */
	gcc_pure
	static string Apply(const_pointer_type base,
			    const_pointer_type path) noexcept;
};

/**
 * This class describes the nature of a MPD internal filesystem path.
 */
struct PathTraitsUTF8 {
	typedef std::string string;
	typedef string::traits_type char_traits;
	typedef char_traits::char_type value_type;
	typedef value_type *pointer_type;
	typedef const value_type *const_pointer_type;

	static constexpr value_type SEPARATOR = '/';

	static constexpr const_pointer_type CURRENT_DIRECTORY = ".";

	static constexpr bool IsSeparator(value_type ch) {
		return ch == SEPARATOR;
	}

	gcc_pure gcc_nonnull_all
	static const_pointer_type FindLastSeparator(const_pointer_type p) noexcept {
#if !CLANG_CHECK_VERSION(3,6)
		/* disabled on clang due to -Wtautological-pointer-compare */
		assert(p != nullptr);
#endif

		return strrchr(p, SEPARATOR);
	}

#ifdef _WIN32
	gcc_pure gcc_nonnull_all
	static constexpr bool IsDrive(const_pointer_type p) noexcept {
		return IsAlphaASCII(p[0]) && p[1] == ':';
	}
#endif

	gcc_pure gcc_nonnull_all
	static bool IsAbsolute(const_pointer_type p) noexcept {
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

	gcc_pure gcc_nonnull_all
	static size_t GetLength(const_pointer_type p) noexcept {
		return StringLength(p);
	}

	gcc_pure gcc_nonnull_all
	static const_pointer_type Find(const_pointer_type p, value_type ch) noexcept {
		return StringFind(p, ch);
	}

	/**
	 * Determine the "base" file name of the given UTF-8 path.
	 * The return value points inside the given string.
	 */
	gcc_pure gcc_nonnull_all
	static const_pointer_type GetBase(const_pointer_type p) noexcept;

	/**
	 * Determine the "parent" file name of the given UTF-8 path.
	 * As a special case, returns the string "." if there is no
	 * separator in the given input string.
	 */
	gcc_pure gcc_nonnull_all
	static string GetParent(const_pointer_type p) noexcept;

	/**
	 * Determine the relative part of the given path to this
	 * object, not including the directory separator.  Returns an
	 * empty string if the given path equals this object or
	 * nullptr on mismatch.
	 */
	gcc_pure gcc_nonnull_all
	static const_pointer_type Relative(const_pointer_type base,
					   const_pointer_type other) noexcept;

	/**
	 * Constructs the path from the given components.
	 * If either of the components is empty string,
	 * remaining component is returned unchanged.
	 * If both components are empty strings, empty string is returned.
	 */
	gcc_pure gcc_nonnull_all
	static string Build(const_pointer_type a, size_t a_size,
			    const_pointer_type b, size_t b_size) noexcept;

	gcc_pure gcc_nonnull_all
	static string Build(const_pointer_type a, const_pointer_type b) noexcept {
		return Build(a, GetLength(a), b, GetLength(b));
	}
};

#endif
