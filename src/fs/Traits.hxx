/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "check.h"
#include "Compiler.h"

#ifdef WIN32
#include "util/CharUtil.hxx"
#endif

#include <string>

#include <string.h>
#include <assert.h>

/**
 * This class describes the nature of a native filesystem path.
 */
struct PathTraitsFS {
	typedef std::string string;
	typedef char value_type;
	typedef value_type *pointer;
	typedef const value_type *const_pointer;

#ifdef WIN32
	static constexpr value_type SEPARATOR = '\\';
#else
	static constexpr value_type SEPARATOR = '/';
#endif

	static constexpr bool IsSeparator(value_type ch) {
		return
#ifdef WIN32
			ch == '/' ||
#endif
			ch == SEPARATOR;
	}

	gcc_pure gcc_nonnull_all
	static const_pointer FindLastSeparator(const_pointer p) {
#if !CLANG_CHECK_VERSION(3,6)
		/* disabled on clang due to -Wtautological-pointer-compare */
		assert(p != nullptr);
#endif

#ifdef WIN32
		const_pointer pos = p + GetLength(p);
		while (p != pos && !IsSeparator(*pos))
			--pos;
		return IsSeparator(*pos) ? pos : nullptr;
#else
		return strrchr(p, SEPARATOR);
#endif
	}

#ifdef WIN32
	gcc_pure gcc_nonnull_all
	static constexpr bool IsDrive(const_pointer p) {
		return IsAlphaASCII(p[0]) && p[1] == ':';
	}
#endif

	gcc_pure gcc_nonnull_all
	static bool IsAbsolute(const_pointer p) {
#if !CLANG_CHECK_VERSION(3,6)
		/* disabled on clang due to -Wtautological-pointer-compare */
		assert(p != nullptr);
#endif

#ifdef WIN32
		if (IsDrive(p) && IsSeparator(p[2]))
			return true;
#endif
		return IsSeparator(*p);
	}

	gcc_pure gcc_nonnull_all
	static size_t GetLength(const_pointer p) {
		return strlen(p);
	}

	/**
	 * Determine the "base" file name of the given native path.
	 * The return value points inside the given string.
	 */
	gcc_pure gcc_nonnull_all
	static const_pointer GetBase(const_pointer p);

	/**
	 * Determine the "parent" file name of the given native path.
	 * As a special case, returns the string "." if there is no
	 * separator in the given input string.
	 */
	gcc_pure gcc_nonnull_all
	static string GetParent(const_pointer p);

	/**
	 * Determine the relative part of the given path to this
	 * object, not including the directory separator.  Returns an
	 * empty string if the given path equals this object or
	 * nullptr on mismatch.
	 */
	gcc_pure gcc_nonnull_all
	static const_pointer Relative(const_pointer base, const_pointer other);

	/**
	 * Constructs the path from the given components.
	 * If either of the components is empty string,
	 * remaining component is returned unchanged.
	 * If both components are empty strings, empty string is returned.
	 */
	gcc_pure gcc_nonnull_all
	static string Build(const_pointer a, size_t a_size,
			    const_pointer b, size_t b_size);

	gcc_pure gcc_nonnull_all
	static string Build(const_pointer a, const_pointer b) {
		return Build(a, GetLength(a), b, GetLength(b));
	}
};

/**
 * This class describes the nature of a MPD internal filesystem path.
 */
struct PathTraitsUTF8 {
	typedef std::string string;
	typedef char value_type;
	typedef value_type *pointer;
	typedef const value_type *const_pointer;

	static constexpr value_type SEPARATOR = '/';

	static constexpr bool IsSeparator(value_type ch) {
		return ch == SEPARATOR;
	}

	gcc_pure gcc_nonnull_all
	static const_pointer FindLastSeparator(const_pointer p) {
#if !CLANG_CHECK_VERSION(3,6)
		/* disabled on clang due to -Wtautological-pointer-compare */
		assert(p != nullptr);
#endif

		return strrchr(p, SEPARATOR);
	}

#ifdef WIN32
	gcc_pure gcc_nonnull_all
	static constexpr bool IsDrive(const_pointer p) {
		return IsAlphaASCII(p[0]) && p[1] == ':';
	}
#endif

	gcc_pure gcc_nonnull_all
	static bool IsAbsolute(const_pointer p) {
#if !CLANG_CHECK_VERSION(3,6)
		/* disabled on clang due to -Wtautological-pointer-compare */
		assert(p != nullptr);
#endif

#ifdef WIN32
		if (IsDrive(p) && IsSeparator(p[2]))
			return true;
#endif
		return IsSeparator(*p);
	}

	gcc_pure gcc_nonnull_all
	static size_t GetLength(const_pointer p) {
		return strlen(p);
	}

	/**
	 * Determine the "base" file name of the given UTF-8 path.
	 * The return value points inside the given string.
	 */
	gcc_pure gcc_nonnull_all
	static const_pointer GetBase(const_pointer p);

	/**
	 * Determine the "parent" file name of the given UTF-8 path.
	 * As a special case, returns the string "." if there is no
	 * separator in the given input string.
	 */
	gcc_pure gcc_nonnull_all
	static string GetParent(const_pointer p);

	/**
	 * Determine the relative part of the given path to this
	 * object, not including the directory separator.  Returns an
	 * empty string if the given path equals this object or
	 * nullptr on mismatch.
	 */
	gcc_pure gcc_nonnull_all
	static const_pointer Relative(const_pointer base, const_pointer other);

	/**
	 * Constructs the path from the given components.
	 * If either of the components is empty string,
	 * remaining component is returned unchanged.
	 * If both components are empty strings, empty string is returned.
	 */
	gcc_pure gcc_nonnull_all
	static string Build(const_pointer a, size_t a_size,
			    const_pointer b, size_t b_size);

	gcc_pure gcc_nonnull_all
	static string Build(const_pointer a, const_pointer b) {
		return Build(a, GetLength(a), b, GetLength(b));
	}
};

#endif
