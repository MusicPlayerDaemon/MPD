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

#ifndef MPD_FS_ALLOCATED_PATH_HXX
#define MPD_FS_ALLOCATED_PATH_HXX

#include "check.h"
#include "Compiler.h"
#include "Traits.hxx"
#include "Path.hxx"

#include <utility>
#include <string>

#include <assert.h>

class Error;

/**
 * A path name in the native file system character set.
 *
 * This class manages the memory chunk where this path string is
 * stored.
 */
class AllocatedPath {
	typedef std::string string;

	typedef PathTraits::value_type value_type;
	typedef PathTraits::pointer pointer;
	typedef PathTraits::const_pointer const_pointer;

	string value;

	struct Donate {};

	/**
	 * Donate the allocated pointer to a new #AllocatedPath object.
	 */
	AllocatedPath(Donate, pointer _value);

	AllocatedPath(const_pointer _value):value(_value) {}

public:
	/**
	 * Copy a #AllocatedPath object.
	 */
	AllocatedPath(const AllocatedPath &) = default;

	/**
	 * Move a #AllocatedPath object.
	 */
	AllocatedPath(AllocatedPath &&other):value(std::move(other.value)) {}

	~AllocatedPath();

	/**
	 * Return a "nulled" instance.  Its IsNull() method will
	 * return true.  Such an object must not be used.
	 *
	 * @see IsNull()
	 */
	gcc_const
	static AllocatedPath Null() {
		return AllocatedPath("");
	}

	gcc_pure
	operator Path() const {
		return Path::FromFS(c_str());
	}

	/**
	 * Join two path components with the path separator.
	 */
	gcc_pure gcc_nonnull_all
	static AllocatedPath Build(const_pointer a, const_pointer b);

	gcc_pure gcc_nonnull_all
	static AllocatedPath Build(const_pointer a, const AllocatedPath &b) {
		return Build(a, b.c_str());
	}

	gcc_pure gcc_nonnull_all
	static AllocatedPath Build(const AllocatedPath &a, const_pointer b) {
		return Build(a.c_str(), b);
	}

	gcc_pure
	static AllocatedPath Build(const AllocatedPath &a,
				   const AllocatedPath &b) {
		return Build(a.c_str(), b.c_str());
	}

	/**
	 * Convert a C string that is already in the filesystem
	 * character set to a #Path instance.
	 */
	gcc_pure
	static AllocatedPath FromFS(const_pointer fs) {
		return AllocatedPath(fs);
	}

	/**
	 * Convert a UTF-8 C string to a #AllocatedPath instance.
	 * Returns return a "nulled" instance on error.
	 */
	gcc_pure gcc_nonnull_all
	static AllocatedPath FromUTF8(const char *path_utf8);

	gcc_pure gcc_nonnull_all
	static AllocatedPath FromUTF8(const char *path_utf8, Error &error);

	/**
	 * Copy a #AllocatedPath object.
	 */
	AllocatedPath &operator=(const AllocatedPath &) = default;

	/**
	 * Move a #AllocatedPath object.
	 */
	AllocatedPath &operator=(AllocatedPath &&other) {
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
	gcc_pure
	std::string ToUTF8() const;

	/**
	 * Gets directory name of this path.
	 * Returns a "nulled" instance on error.
	 */
	gcc_pure
	AllocatedPath GetDirectoryName() const;

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

	gcc_pure
	bool IsAbsolute() {
		return PathTraits::IsAbsoluteFS(c_str());
	}
};

#endif
