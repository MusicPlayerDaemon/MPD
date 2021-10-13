/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
