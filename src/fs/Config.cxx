// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Config.hxx"
#include "Charset.hxx"
#include "Features.hxx"
#include "config/Data.hxx"
#include "config.h"

void
ConfigureFS(const ConfigData &config)
{
#ifdef HAVE_FS_CHARSET
	const char *charset = config.GetString(ConfigOption::FS_CHARSET);
	if (charset != nullptr)
		SetFSCharset(charset);
#else
	(void)config;
#endif
}

void
DeinitFS() noexcept
{
#ifdef HAVE_FS_CHARSET
	DeinitFSCharset();
#endif
}
