// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Version.hxx"

#include <alsa/asoundlib.h>

#include <stdlib.h>

[[gnu::pure]]
static uint_least32_t
ParseAlsaVersion(const char *p) noexcept
{
	char *endptr;
	unsigned long major, minor = 0, subminor = 0;

	major = strtoul(p, &endptr, 10);
	if (*endptr == '.') {
		p = endptr + 1;
		minor = strtoul(p, &endptr, 10);
		if (*endptr == '.') {
			p = endptr + 1;
			subminor = strtoul(p, nullptr, 10);
		}
	}

	return MakeAlsaVersion(major, minor, subminor);
}

uint_least32_t
GetRuntimeAlsaVersion() noexcept
{
	const char *version = snd_asoundlib_version();
	if (version == nullptr)
		return 0;

	return ParseAlsaVersion(version);
}
