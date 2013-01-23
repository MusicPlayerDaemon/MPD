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

#include <glib.h>

#include <algorithm>
#include <string>

#include <assert.h>
#include <string.h>
#include <limits.h>

#if !defined(MPD_PATH_MAX)
#  if defined(MAXPATHLEN)
#    define MPD_PATH_MAX MAXPATHLEN
#  elif defined(PATH_MAX)
#    define MPD_PATH_MAX PATH_MAX
#  else
#    define MPD_PATH_MAX 256
#  endif
#endif

void path_global_init();

void path_global_finish();

/**
 * Converts a file name in the filesystem charset to UTF-8.  Returns
 * NULL on failure.
 */
char *
fs_charset_to_utf8(const char *path_fs);

/**
 * Converts a file name in UTF-8 to the filesystem charset.  Returns a
 * duplicate of the UTF-8 string on failure.
 */
char *
utf8_to_fs_charset(const char *path_utf8);

const char *path_get_fs_charset();

/**
 * A path name in the native file system character set.
 */
class Path {
public:
	typedef char value_type;
	typedef value_type *pointer;
	typedef const value_type *const_pointer;

private:
	pointer value;

	struct Donate {};

	/**
	 * Donate the allocated pointer to a new #Path object.
	 */
	constexpr Path(Donate, pointer _value):value(_value) {}

	/**
	 * Release memory allocated by the value, but do not clear the
	 * value pointer.
	 */
	void Free() {
		/* free() can be optimized by gcc, while g_free() can
		   not: when the compiler knows that the value is
		   nullptr, it will not emit a free() call in the
		   inlined destructor; however on Windows, we need to
		   call g_free(), because the value has been allocated
		   by GLib, and on Windows, this matters */
#ifdef WIN32
		g_free(value);
#else
		free(value);
#endif
	}

public:
	/**
	 * Copy a #Path object.
	 */
	Path(const Path &other)
		:value(g_strdup(other.value)) {}

	/**
	 * Move a #Path object.
	 */
	Path(Path &&other):value(other.value) {
		other.value = nullptr;
	}

	~Path() {
		Free();
	}

	/**
	 * Return a "nulled" instance.  Its IsNull() method will
	 * return true.  Such an object must not be used.
	 *
	 * @see IsNull()
	 */
	gcc_const
	static Path Null() {
		return Path(Donate(), nullptr);
	}

	/**
	 * Join two path components with the path separator.
	 */
	gcc_pure gcc_nonnull_all
	static Path Build(const_pointer a, const_pointer b) {
		return Path(Donate(), g_build_filename(a, b, nullptr));
	}

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
		return Path(Donate(), g_strdup(fs));
	}

	/**
	 * Convert a UTF-8 C string to a #Path instance.
	 *
	 * TODO: return a "nulled" instance on error and add checks to
	 * all callers
	 */
	gcc_pure
	static Path FromUTF8(const char *utf8) {
		return Path(Donate(), utf8_to_fs_charset(utf8));
	}

	/**
	 * Copy a #Path object.
	 */
	Path &operator=(const Path &other) {
		if (this != &other) {
			Free();
			value = g_strdup(other.value);
		}

		return *this;
	}

	/**
	 * Move a #Path object.
	 */
	Path &operator=(Path &&other) {
		std::swap(value, other.value);
		return *this;
	}

	/**
	 * Steal the allocated value.  This object has an undefined
	 * value, and the caller is response for freeing this method's
	 * return value.
	 */
	pointer Steal() {
		pointer result = value;
		value = nullptr;
		return result;
	}

	/**
	 * Check if this is a "nulled" instance.  A "nulled" instance
	 * must not be used.
	 */
	bool IsNull() const {
		return value == nullptr;
	}

	/**
	 * Clear this object's value, make it "nulled".
	 *
	 * @see IsNull()
	 */
	void SetNull() {
		Free();
		value = nullptr;
	}

	gcc_pure
	bool empty() const {
		assert(value != nullptr);

		return *value == 0;
	}

	/**
	 * @return the length of this string in number of "value_type"
	 * elements (which may not be the number of characters).
	 */
	gcc_pure
	size_t length() const {
		assert(value != nullptr);

		return strlen(value);
	}

	/**
	 * Returns the value as a const C string.  The returned
	 * pointer is invalidated whenever the value of life of this
	 * instance ends.
	 */
	gcc_pure
	const_pointer c_str() const {
		assert(value != nullptr);

		return value;
	}

	/**
	 * Convert the path to UTF-8.
	 * Returns empty string on error or if this instance is "nulled"
	 * (#IsNull returns true).
	 */
	std::string ToUTF8() const;
};

#endif
