// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifdef _WIN32
// COM needs the "MSG" typedef, and shlwapi.h includes COM headers
#undef NOUSER
#endif

#include "Glob.hxx"

#ifdef _WIN32
#include <shlwapi.h>

bool
Glob::Check(const char *name_fs) const noexcept
{
	return PathMatchSpecA(name_fs, pattern.c_str());
}

#endif
