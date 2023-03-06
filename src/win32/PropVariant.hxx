// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_WIN32_PROPVARIANT_HXX
#define MPD_WIN32_PROPVARIANT_HXX

#include <combaseapi.h> // needed by propidl.h if COM_NO_WINDOWS_H is defined
#include <propidl.h>

class AllocatedString;

[[gnu::pure]]
AllocatedString
ToString(const PROPVARIANT &pv) noexcept;

#endif
