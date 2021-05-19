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

#ifndef MPD_WIN32_COM_HXX
#define MPD_WIN32_COM_HXX

#include "HResult.hxx"

#include <combaseapi.h>
#include <objbase.h> // for COINIT_APARTMENTTHREADED

// RAII for Microsoft Component Object Model(COM)
// https://docs.microsoft.com/en-us/windows/win32/api/_com/
class COM {
public:
	COM() {
		if (HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
		    FAILED(result)) {
			throw MakeHResultError(
				result,
				"Unable to initialize COM with COINIT_APARTMENTTHREADED");
		}
	}
	~COM() noexcept { CoUninitialize(); }

	COM(const COM &) = delete;
	COM(COM &&) = delete;
	COM &operator=(const COM &) = delete;
	COM &operator=(COM &&) = delete;
};

#endif
