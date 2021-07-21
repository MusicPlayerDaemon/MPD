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

#include "NarrowPath.hxx"

#ifdef _UNICODE

#include "lib/icu/Win32.hxx"
#include "system/Error.hxx"

#include <windows.h>

NarrowPath::NarrowPath(Path _path) noexcept
	:value(WideCharToMultiByte(CP_ACP, _path.c_str()))
{
	if (value == nullptr)
		/* fall back to empty string */
		value = Value::Empty();
}

static AllocatedPath
AcpToAllocatedPath(const char *s)
{
	wchar_t buffer[MAX_PATH];
	auto result = MultiByteToWideChar(CP_ACP, 0, s, -1,
					  buffer, std::size(buffer));
	if (result <= 0)
		throw MakeLastError("MultiByteToWideChar() failed");

	return AllocatedPath::FromFS(std::wstring_view(buffer, result));
}

FromNarrowPath::FromNarrowPath(const char *s)
	:value(AcpToAllocatedPath(s))
{
}

#endif /* _UNICODE */
