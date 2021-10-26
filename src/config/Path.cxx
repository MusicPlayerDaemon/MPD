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

#include "Path.hxx"
#include "Data.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/StandardDirectory.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringView.hxx"

#include <cassert>

#ifndef _WIN32
#include <pwd.h>

static const char *configured_user = nullptr;

/**
 * Determine a given user's home directory.
 */
static AllocatedPath
GetHome(const char *user)
{
	AllocatedPath result = GetHomeDir(user);
	if (result.IsNull())
		throw FormatRuntimeError("no such user: %s", user);

	return result;
}

/**
 * Determine the current user's home directory.
 */
static AllocatedPath
GetHome()
{
	AllocatedPath result = GetHomeDir();
	if (result.IsNull())
		throw std::runtime_error("problems getting home for current user");

	return result;
}

/**
 * Determine the configured user's home directory.
 *
 * Throws #std::runtime_error on error.
 */
static AllocatedPath
GetConfiguredHome()
{
	return configured_user != nullptr
		? GetHome(configured_user)
		: GetHome();
}

#endif

void
InitPathParser(const ConfigData &config) noexcept
{
#ifdef _WIN32
	(void)config;
#else
	configured_user = config.GetString(ConfigOption::USER);
#endif
}

AllocatedPath
ParsePath(const char *path)
{
	assert(path != nullptr);

#ifndef _WIN32
	if (path[0] == '~') {
		++path;

		if (*path == '\0')
			return GetConfiguredHome();

		if (*path == '/') {
			++path;

			return GetConfiguredHome() /
				AllocatedPath::FromUTF8Throw(path);
		} else {
			const auto [user, rest] =
				StringView{path}.Split('/');

			return GetHome(std::string{user}.c_str())
				/ AllocatedPath::FromUTF8Throw(rest);
		}
	} else if (!PathTraitsUTF8::IsAbsolute(path)) {
		throw FormatRuntimeError("not an absolute path: %s", path);
	} else {
#endif
		return AllocatedPath::FromUTF8Throw(path);
#ifndef _WIN32
	}
#endif
}
