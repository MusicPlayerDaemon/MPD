// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
