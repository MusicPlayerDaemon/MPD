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

#include "PropVariant.hxx"
#include "lib/icu/Win32.hxx"
#include "util/AllocatedString.hxx"
#include "util/ScopeExit.hxx"

AllocatedString
ToString(const PROPVARIANT &pv) noexcept
{
	// TODO: VT_BSTR

	switch (pv.vt) {
	case VT_LPSTR:
		return AllocatedString{static_cast<const char *>(pv.pszVal)};

	case VT_LPWSTR:
		return WideCharToMultiByte(CP_UTF8, pv.pwszVal);

	default:
		return nullptr;
	}
}
