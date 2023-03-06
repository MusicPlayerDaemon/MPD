// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
