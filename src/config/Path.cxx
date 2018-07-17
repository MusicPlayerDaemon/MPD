/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "Path.hxx"
#include "Global.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/Domain.hxx"
#include "fs/StandardDirectory.hxx"
#include "util/RuntimeError.hxx"

#include <assert.h>
#include <string.h>

#ifndef _WIN32
#include <pwd.h>

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
	const char *user = config_get_string(ConfigOption::USER);
	return user != nullptr
		? GetHome(user)
		: GetHome();
}

#endif

AllocatedPath
ParsePath(const char *path)
{
	assert(path != nullptr);

#ifndef _WIN32
	if (path[0] == '~') {
		++path;

		if (*path == '\0')
			return GetConfiguredHome();

		AllocatedPath home = nullptr;

		if (*path == '/') {
			home = GetConfiguredHome();

			++path;
		} else {
			const char *slash = strchr(path, '/');
			const char *end = slash == nullptr
					? path + strlen(path)
					: slash;
			const std::string user(path, end);
			home = GetHome(user.c_str());

			if (slash == nullptr)
				return home;

			path = slash + 1;
		}

		if (home.IsNull())
			return nullptr;

		AllocatedPath path2 = AllocatedPath::FromUTF8Throw(path);
		if (path2.IsNull())
			return nullptr;

		return home / path2;
	} else if (!PathTraitsUTF8::IsAbsolute(path)) {
		throw FormatRuntimeError("not an absolute path: %s", path);
	} else {
#endif
		return AllocatedPath::FromUTF8Throw(path);
#ifndef _WIN32
	}
#endif
}
