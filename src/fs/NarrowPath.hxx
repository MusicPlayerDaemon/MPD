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

#ifndef MPD_FS_NARROW_PATH_HXX
#define MPD_FS_NARROW_PATH_HXX

#include "Path.hxx"

#ifdef _UNICODE
#include "lib/icu/Win32.hxx"
#include "util/AllocatedString.hxx"
#include <windows.h>
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
	typedef AllocatedString<> Value;
#else
	typedef StringPointer<> Value;
#endif
	typedef typename Value::const_pointer_type const_pointer_type;

	Value value;

public:
#ifdef _UNICODE
	explicit NarrowPath(Path _path)
		:value(WideCharToMultiByte(CP_ACP, _path.c_str())) {
		if (value.IsNull())
			/* fall back to empty string */
			value = Value::Empty();
	}
#else
	explicit NarrowPath(Path _path):value(_path.c_str()) {}
#endif

	operator const_pointer_type() const {
		return c_str();
	}

	const_pointer_type c_str() const {
		return value.c_str();
	}
};

#endif
