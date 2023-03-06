// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
