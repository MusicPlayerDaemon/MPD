/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#ifndef MPD_FS_PATH_HXX
#define MPD_FS_PATH_HXX

#include "check.h"
#include "gcc.h"

#include <algorithm>
#include <string>

#include <assert.h>
#include <string.h>
#include <limits.h>

#if !defined(MPD_PATH_MAX)
#  if defined(WIN32)
#    define MPD_PATH_MAX 260
#  elif defined(MAXPATHLEN)
#    define MPD_PATH_MAX MAXPATHLEN
#  elif defined(PATH_MAX)
#    define MPD_PATH_MAX PATH_MAX
#  else
#    define MPD_PATH_MAX 256
#  endif
#endif

class Error;

extern const class Domain path_domain;

/**
 * A path name in the native file system character set.
 */
class Path {
	typedef std::string string;

public:
	typedef string::value_type value_type;
	typedef string::pointer pointer;
	typedef string::const_pointer const_pointer;

#ifdef WIN32
	static constexpr value_type SEPARATOR_FS = '\\';
	static constexpr char SEPARATOR_UTF8 = '/';
#else
	static constexpr value_type SEPARATOR_FS = '/';
	static constexpr char SEPARATOR_UTF8 = '/';
#endif

private:
	string value;

	struct Donate {};

	/**
	 * Donate the allocated pointer to a new #Path object.
	 */
	Path(Donate, pointer _value);

	Path(const_pointer _value):value(_value) {}

public:
	/**
	 * Copy a #Path object.
	 */
	Path(const Path &) = default;

	/**
	 * Move a #Path object.
	 */
	Path(Path &&other):value(std::move(other.value)) {}

	~Path();

	/**
	 * Return a "nulled" instance.  Its IsNull() method will
	 * return true.  Such an object must not be used.
	 *
	 * @see IsNull()
	 */
	gcc_const
	static Path Null() {
		return Path("");
	}

	/**
	 * Join two path components with the path separator.
	 */
	gcc_pure gcc_nonnull_all
	static Path Build(const_pointer a, const_pointer b);

	gcc_pure gcc_nonnull_all
	static Path Build(const_pointer a, const Path &b) {
		return Build(a, b.c_str());
	}

	gcc_pure gcc_nonnull_all
	static Path Build(const Path &a, const_pointer b) {
		return Build(a.c_str(), b);
	}

	gcc_pure
	static Path Build(const Path &a, const Path &b) {
		return Build(a.c_str(), b.c_str());
	}

	/**
	 * Convert a C string that is already in the filesystem
	 * character set to a #Path instance.
	 */
	gcc_pure
	static Path FromFS(const_pointer fs) {
		return Path(fs);
	}

	/**
	 * Convert a UTF-8 C string to a #Path instance.
	 * Returns return a "nulled" instance on error.
	 */
	gcc_pure
	static Path FromUTF8(const char *path_utf8);

	gcc_pure
	static Path FromUTF8(const char *path_utf8, Error &error);

	/**
	 * Convert the path to UTF-8.
	 * Returns empty string on error or if #path_fs is null pointer.
	 */
	gcc_pure
	static std::string ToUTF8(const_pointer path_fs);

	/**
	 * Performs global one-time initialization of this class.
	 */
	static void GlobalInit();

	/**
	 * Gets file system character set name.
	 */
	static const std::string &GetFSCharset();

	/**
	 * Copy a #Path object.
	 */
	Path &operator=(const Path &) = default;

	/**
	 * Move a #Path object.
	 */
	Path &operator=(Path &&other) {
		value = std::move(other.value);
		return *this;
	}

	/**
	 * Check if this is a "nulled" instance.  A "nulled" instance
	 * must not be used.
	 */
	bool IsNull() const {
		return value.empty();
	}

	/**
	 * Clear this object's value, make it "nulled".
	 *
	 * @see IsNull()
	 */
	void SetNull() {
		value.clear();
	}

	/**
	 * @return the length of this string in number of "value_type"
	 * elements (which may not be the number of characters).
	 */
	gcc_pure
	size_t length() const {
		return value.length();
	}

	/**
	 * Returns the value as a const C string.  The returned
	 * pointer is invalidated whenever the value of life of this
	 * instance ends.
	 */
	gcc_pure
	const_pointer c_str() const {
		return value.c_str();
	}

	/**
	 * Returns a pointer to the raw value, not necessarily
	 * null-terminated.
	 */
	gcc_pure
	const_pointer data() const {
		return value.data();
	}

	/**
	 * Convert the path to UTF-8.
	 * Returns empty string on error or if this instance is "nulled"
	 * (#IsNull returns true).
	 */
	std::string ToUTF8() const {
		return ToUTF8(value.c_str());
	}

	/**
	 * Gets directory name of this path.
	 * Returns a "nulled" instance on error.
	 */
	gcc_pure
	Path GetDirectoryName() const;

	/**
	 * Determine the relative part of the given path to this
	 * object, not including the directory separator.  Returns an
	 * empty string if the given path equals this object or
	 * nullptr on mismatch.
	 */
	gcc_pure
	const char *RelativeFS(const char *other_fs) const;

	/**
	 * Chop trailing directory separators.
	 */
	void ChopSeparators();

	static constexpr bool IsSeparatorFS(value_type ch) {
		return
#ifdef WIN32
			ch == '/' ||
#endif
			ch == SEPARATOR_FS;
	}

	static constexpr bool IsSeparatorUTF8(char ch) {
		return
#ifdef WIN32
			ch == '/' ||
#endif
			ch == SEPARATOR_UTF8;
	}
};

#endif
