// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FS_NARROW_PATH_HXX
#define MPD_FS_NARROW_PATH_HXX

#include "Path.hxx"

#ifdef _UNICODE
#include "AllocatedPath.hxx"
#include "util/AllocatedString.hxx"
#else
#include "util/StringPointer.hxx"
#endif

/**
 * A path name that uses the regular (narrow) "char".  This is used to
 * pass a #Path (which may be represented by wchar_t) to a library
 * that accepts only "const char *".
 */
class NarrowPath {
#ifdef _UNICODE
	using Value = AllocatedString;
#else
	using Value = StringPointer<>;
#endif
	using const_pointer = typename Value::const_pointer;

	Value value;

public:
#ifdef _UNICODE
	explicit NarrowPath(Path _path) noexcept;
#else
	explicit NarrowPath(Path _path):value(_path.c_str()) {}
#endif

	operator const_pointer() const {
		return c_str();
	}

	const_pointer c_str() const {
		return value.c_str();
	}
};

/**
 * A path name converted from a "narrow" string.  This is used to
 * import an existing narrow string to a #Path.
 */
class FromNarrowPath {
#ifdef _UNICODE
	using Value = AllocatedPath;
#else
	using Value = Path;
#endif

	Value value{nullptr};

public:
	FromNarrowPath() = default;

#ifdef _UNICODE
	/**
	 * Throws on error.
	 */
	FromNarrowPath(const char *s);
#else
	constexpr FromNarrowPath(const char *s) noexcept
		:value(Value::FromFS(s)) {}
#endif

#ifndef _UNICODE
	constexpr
#endif
	operator Path() const noexcept {
#ifdef _UNICODE
		if (value.IsNull())
			return nullptr;
#endif

		return value;
	}
};

#endif
