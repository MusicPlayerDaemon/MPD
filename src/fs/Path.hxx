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

#ifndef MPD_FS_PATH_HXX
#define MPD_FS_PATH_HXX

#include "Traits.hxx"

#include <cassert>
#include <string>

class AllocatedPath;

/**
 * A path name in the native file system character set.
 *
 * This class manages a pointer to an existing path string.  While an
 * instance lives, the string must not be invalidated.
 */
class Path : public PathTraitsFS::Pointer {
	using Traits = PathTraitsFS;
	using Base = Traits::Pointer;

	explicit constexpr Path(const_pointer _value) noexcept:Base(_value) {}

public:
	/**
	 * Construct a "nulled" instance.  Its IsNull() method will
	 * return true.  Such an object must not be used.
	 *
	 * @see IsNull()
	 */
	constexpr Path(std::nullptr_t) noexcept:Base(nullptr) {}

	/**
	 * Copy a #Path object.
	 */
	constexpr Path(const Path &) = default;

	/**
	 * Create a new instance pointing to the specified path
	 * string.
	 */
	static constexpr Path FromFS(const_pointer fs) noexcept {
		return Path(fs);
	}

	/**
	 * Copy a #Path object.
	 */
	Path &operator=(const Path &) = default;

	/**
	 * Check if this is a "nulled" instance.  A "nulled" instance
	 * must not be used.
	 */
	constexpr bool IsNull() const noexcept {
		return Base::IsNull();
	}

	/**
	 * Clear this object's value, make it "nulled".
	 *
	 * @see IsNull()
	 */
	void SetNull() noexcept {
		*this = nullptr;
	}

	/**
	 * @return the length of this string in number of "value_type"
	 * elements (which may not be the number of characters).
	 */
	[[gnu::pure]]
	size_t length() const noexcept {
		assert(!IsNull());

		return Traits::GetLength(c_str());
	}

	/**
	 * Returns the value as a const C string.  The returned
	 * pointer is invalidated whenever the value of life of this
	 * instance ends.
	 */
	constexpr const_pointer c_str() const noexcept {
		return Base::c_str();
	}

	/**
	 * Returns a pointer to the raw value, not necessarily
	 * null-terminated.
	 */
	constexpr const_pointer data() const noexcept {
		return c_str();
	}

	/**
	 * Does the path contain a newline character?  (Which is
	 * usually rejected by MPD because its protocol cannot
	 * transfer newline characters).
	 */
	[[gnu::pure]]
	bool HasNewline() const noexcept {
		return Traits::Find(c_str(), '\n') != nullptr;
	}

	/**
	 * Convert the path to UTF-8.
	 * Returns empty string on error or if this instance is "nulled"
	 * (#IsNull returns true).
	 */
	[[gnu::pure]]
	std::string ToUTF8() const noexcept;

	/**
	 * Like ToUTF8(), but throws on error.
	 */
	std::string ToUTF8Throw() const;

	/**
	 * Determine the "base" file name.
	 * The return value points inside this object.
	 */
	[[gnu::pure]]
	Path GetBase() const noexcept {
		return FromFS(Traits::GetBase(c_str()));
	}

	/**
	 * Gets directory name of this path.
	 * Returns a "nulled" instance on error.
	 */
	[[gnu::pure]]
	AllocatedPath GetDirectoryName() const noexcept;

	/**
	 * Determine the relative part of the given path to this
	 * object, not including the directory separator.  Returns an
	 * empty string if the given path equals this object or
	 * nullptr on mismatch.
	 */
	[[gnu::pure]]
	const_pointer Relative(Path other_fs) const noexcept {
		return Traits::Relative(c_str(), other_fs.c_str());
	}

	[[gnu::pure]]
	bool IsAbsolute() const noexcept {
		return Traits::IsAbsolute(c_str());
	}

	[[gnu::pure]]
	const_pointer GetSuffix() const noexcept;
};

/**
 * Concatenate two path components using the directory separator.
 *
 * Wrapper for AllocatedPath::Build().
 */
AllocatedPath
operator/(Path a, Path b) noexcept;

#endif
