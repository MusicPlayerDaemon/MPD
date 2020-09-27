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

/*
 * Support library for the "idle" command.
 *
 */

#include "IdleFlags.hxx"
#include "util/ASCII.hxx"

#include <cassert>

static constexpr const char * idle_names[] = {
	"database",
	"stored_playlist",
	"playlist",
	"player",
	"mixer",
	"output",
	"options",
	"sticker",
	"update",
	"subscription",
	"message",
	"neighbor",
	"mount",
	"partition",
	nullptr,
};

const char*const*
idle_get_names() noexcept
{
        return idle_names;
}

unsigned
idle_parse_name(const char *name) noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(name != nullptr);
#endif

	for (unsigned i = 0; idle_names[i] != nullptr; ++i)
		if (StringEqualsCaseASCII(name, idle_names[i]))
			return 1 << i;

	return 0;
}
