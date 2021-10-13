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

#ifndef MPD_FS_ALLOCATED_PATH_HXX
#define MPD_FS_ALLOCATED_PATH_HXX

#include "Traits.hxx"
#include "Path.hxx"

#include <cstddef>
#include <utility>
#include <string>

/**
 * A path name in the native file system character set.
 *
 * This class manages the memory chunk where this path string is
 * stored.
 */
class AllocatedPath {
	using Traits = PathTraitsFS;
	using string = Traits::string;
	using string_view = Traits::string_view;
	using value_type = Traits::value_type;
	using pointer = Traits::pointer;
	using const_pointer = Traits::const_pointer;

	string value;

	explicit AllocatedPath(const_pointer _value) noexcept
		:value(_value) {}

	explicit AllocatedPath(string_view _value) noexcept
		:value(_value) {}

	AllocatedPath(const_pointer _begin, const_pointer _end) noexcept
		:value(_begin, _end) {}

	AllocatedPath(string &&_value) noexcept
		:value(std::move(_value)) {}

public:
	/**
	 * Construct a "nulled" instance.  Its IsNull() method will
	 * return true.  Such an object must not be used.
	 *
	 * @see IsNull()
	 */
	AllocatedPath(std::nullptr_t) noexcept:value() {}

	/**
	 * Copy an #AllocatedPath object.
	 */
	AllocatedPath(const AllocatedPath &) = default;

	/**
	 * Move an #AllocatedPath object.
	 */
	AllocatedPath(AllocatedPath &&other) noexcept
		:value(std::move(other.value)) {}

	explicit AllocatedPath(Path other) noexcept
		:value(other.c_str()) {}

	~AllocatedPath() noexcept;

	[[gnu::pure]]
	operator Path() const noexcept {
		return Path::FromFS(c_str());
	}

	/**
	 * Join two path components with the path separator.
	 */
	[[gnu::pure]]
	static AllocatedPath Build(string_view a, string_view b) noexcept {
		return AllocatedPath(Traits::Build(a, b));
	}

	[[gnu::pure]]
	static AllocatedPath Build(Path a, string_view b) noexcept {
		return Build(a.c_str(), b);
	}

	[[gnu::pure]]
	static AllocatedPath Build(Path a, Path b) noexcept {
		return Build(a, b.c_str());
	}

	[[gnu::pure]]
	static AllocatedPath Build(string_view a,
				   const AllocatedPath &b) noexcept {
		return Build(a, b.value);
	}

	[[gnu::pure]]
	static AllocatedPath Build(const AllocatedPath &a,
				   string_view b) noexcept {
		return Build(a.value, b);
	}

	[[gnu::pure]]
	static AllocatedPath Build(const AllocatedPath &a,
				   const AllocatedPath &b) noexcept {
		return Build(a.value, b.value);
	}

	[[gnu::pure]]
	static AllocatedPath Apply(Path base, Path path) noexcept {
		return Traits::Apply(base.c_str(), path.c_str());
	}

	/**
	 * Convert a C string that is already in the filesystem
	 * character set to a #Path instance.
	 */
	[[gnu::pure]]
	static AllocatedPath FromFS(const_pointer fs) noexcept {
		return AllocatedPath(fs);
	}

	[[gnu::pure]]
	static AllocatedPath FromFS(string_view fs) noexcept {
		return AllocatedPath(fs);
	}

	[[gnu::pure]]
	static AllocatedPath FromFS(const_pointer _begin,
				    const_pointer _end) noexcept {
		return AllocatedPath(_begin, _end);
	}

	/**
	 * Convert a C++ string that is already in the filesystem
	 * character set to a #Path instance.
	 */
	[[gnu::pure]]
	static AllocatedPath FromFS(string &&fs) noexcept {
		return AllocatedPath(std::move(fs));
	}

#ifdef ANDROID
	[[gnu::pure]]
	static AllocatedPath FromUTF8(std::string &&utf8) noexcept {
		/* on Android, the filesystem charset is hard-coded to
		   UTF-8 */
		/* note: we should be using FS_CHARSET_ALWAYS_UTF8
		   here, but that would require adding a dependency on
		   header Features.hxx which I'd like to avoid for
		   now */
		return FromFS(std::move(utf8));
	}
#endif

	/**
	 * Convert a UTF-8 C string to an #AllocatedPath instance.
	 * Returns return a "nulled" instance on error.
	 */
	[[gnu::pure]]
	static AllocatedPath FromUTF8(std::string_view path_utf8) noexcept;

	static AllocatedPath FromUTF8(const char *path_utf8) noexcept {
		return FromUTF8(std::string_view(path_utf8));
	}

	/**
	 * Convert a UTF-8 C string to an #AllocatedPath instance.
	 * Throws a std::runtime_error on error.
	 */
	static AllocatedPath FromUTF8Throw(std::string_view path_utf8);

	/**
	 * Copy an #AllocatedPath object.
	 */
	AllocatedPath &operator=(const AllocatedPath &) = default;

	/**
	 * Move an #AllocatedPath object.
	 */
	AllocatedPath &operator=(AllocatedPath &&other) noexcept {
		value = std::move(other.value);
		return *this;
	}

	[[gnu::pure]]
	bool operator==(const AllocatedPath &other) const noexcept {
		return value == other.value;
	}

	[[gnu::pure]]
	bool operator!=(const AllocatedPath &other) const noexcept {
		return value != other.value;
	}

	/**
	 * Allows the caller to "steal" the internal value by
	 * providing a rvalue reference to the std::string attribute.
	 */
	string &&Steal() noexcept {
		return std::move(value);
	}

	/**
	 * Check if this is a "nulled" instance.  A "nulled" instance
	 * must not be used.
	 */
	bool IsNull() const noexcept {
		return value.empty();
	}

	/**
	 * Clear this object's value, make it "nulled".
	 *
	 * @see IsNull()
	 */
	void SetNull() noexcept {
		value.clear();
	}

	/**
	 * @return the length of this string in number of "value_type"
	 * elements (which may not be the number of characters).
	 */
	[[gnu::pure]]
	size_t length() const noexcept {
		return value.length();
	}

	/**
	 * Returns the value as a const C string.  The returned
	 * pointer is invalidated whenever the value of life of this
	 * instance ends.
	 */
	[[gnu::pure]]
	const_pointer c_str() const noexcept {
		return value.c_str();
	}

	/**
	 * Returns a pointer to the raw value, not necessarily
	 * null-terminated.
	 */
	[[gnu::pure]]
	const_pointer data() const noexcept {
		return value.data();
	}

	/**
	 * Convert the path to UTF-8.
	 * Returns empty string on error or if this instance is "nulled"
	 * (#IsNull returns true).
	 */
	[[gnu::pure]]
	std::string ToUTF8() const noexcept {
		return ((Path)*this).ToUTF8();
	}

	std::string ToUTF8Throw() const {
		return ((Path)*this).ToUTF8Throw();
	}

	/**
	 * Gets directory name of this path.
	 * Returns a "nulled" instance on error.
	 */
	[[gnu::pure]]
	AllocatedPath GetDirectoryName() const noexcept {
		return ((Path)*this).GetDirectoryName();
	}

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
	const_pointer GetSuffix() const noexcept {
		return ((Path)*this).GetSuffix();
	}

	/**
	 * Chop trailing directory separators.
	 */
	void ChopSeparators() noexcept;

	[[gnu::pure]]
	bool IsAbsolute() const noexcept {
		return Traits::IsAbsolute(c_str());
	}
};

#endif
