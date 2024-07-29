// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Support library for the "idle" command.
 *
 */

#include "protocol/IdleFlags.hxx"
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
