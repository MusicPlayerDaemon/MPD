/*
 * Copyright 2020-2021 The Music Player Daemon Project
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

#include "HResult.hxx"

#include <cstdarg>
#include <memory>

std::system_error
FormatHResultError(HRESULT result, const char *fmt, ...) noexcept
{
	std::va_list args1, args2;
	va_start(args1, fmt);
	va_copy(args2, args1);

	const int size = vsnprintf(nullptr, 0, fmt, args1);
	va_end(args1);
	assert(size >= 0);

	auto buffer = std::make_unique<char[]>(size + 1);
	vsprintf(buffer.get(), fmt, args2);
	va_end(args2);

	return std::system_error(std::error_code(result, hresult_category()),
				 std::string(buffer.get(), size));
}
