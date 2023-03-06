// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
