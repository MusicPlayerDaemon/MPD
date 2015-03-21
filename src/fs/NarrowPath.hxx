/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#include "check.h"
#include "Path.hxx"
#include "util/Macros.hxx"

#ifdef _UNICODE
#include <windows.h>
#endif

/**
 * A path name that uses the regular (narrow) "char".  This is used to
 * pass a #Path (which may be represented by wchar_t) to a library
 * that accepts only "const char *".
 */
class NarrowPath {
	typedef char value_type;
	typedef const char *const_pointer;

#ifdef _UNICODE
	char value[PATH_MAX];
#else
	const_pointer value;
#endif

public:
#ifdef _UNICODE
	explicit NarrowPath(Path _path) {
		auto result = WideCharToMultiByte(CP_ACP, 0,
						  _path.c_str(), -1,
						  value, ARRAY_SIZE(value),
						  nullptr, nullptr);
		if (result < 0)
			value[0] = 0;
	}
#else
	explicit NarrowPath(Path _path):value(_path.c_str()) {}
#endif

	operator const_pointer() const {
		return value;
	}

	const_pointer c_str() const {
		return value;
	}
};

#endif
