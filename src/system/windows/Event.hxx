// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <handleapi.h>
#include <synchapi.h>
#include <windef.h> // for HWND (needed by winbase.h)
#include <winbase.h> // for INFINITE

/**
 * RAII for Windows unnamed event object.
 *
 * @see https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createeventw
 */
class WinEvent {
	HANDLE event;

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
};
