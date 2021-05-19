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

#ifndef MPD_WIN32_WINEVENT_HXX
#define MPD_WIN32_WINEVENT_HXX

#include <handleapi.h>
#include <synchapi.h>
#include <windef.h> // for HWND (needed by winbase.h)
#include <winbase.h> // for INFINITE

// RAII for Windows unnamed event object
// https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createeventw

class WinEvent {
public:
	/**
	 * Throws on error.
	 */
	WinEvent();

	~WinEvent() noexcept { CloseHandle(event); }
	WinEvent(WinEvent &&) = delete;
	WinEvent(const WinEvent &) = delete;
	WinEvent &operator=(WinEvent &&) = delete;
	WinEvent &operator=(const WinEvent &) = delete;

	HANDLE handle() noexcept { return event; }

	DWORD Wait(DWORD milliseconds=INFINITE) noexcept {
		return WaitForSingleObject(event, milliseconds);
	}

	bool Set() noexcept { return SetEvent(event); }

private:
	HANDLE event;
};

#endif
