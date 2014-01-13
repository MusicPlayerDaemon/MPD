/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "ConfigPath.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/Domain.hxx"
#include "fs/StandardDirectory.hxx"
#include "util/Error.hxx"
#include "ConfigGlobal.hxx"

#include <assert.h>
#include <string.h>

#ifndef WIN32
#include <pwd.h>

/**
 * Determine a given user's home directory.
 */
static AllocatedPath
GetHome(const char *user, Error &error)
{
	AllocatedPath result = GetHomeDir(user);
	if (result.IsNull()) {
		error.Format(path_domain,
			     "no such user: %s", user);
		return AllocatedPath::Null();
	}

	return result;
}

/**
 * Determine the current user's home directory.
 */
static AllocatedPath
GetHome(Error &error)
{
	AllocatedPath result = GetHomeDir();
	if (result.IsNull()) {
		error.Set(path_domain,
			  "problems getting home for current user");
		return AllocatedPath::Null();
	}

	return result;
}

/**
 * Determine the configured user's home directory.
 */
static AllocatedPath
GetConfiguredHome(Error &error)
{
	const char *user = config_get_string(CONF_USER, nullptr);
	return user != nullptr
		? GetHome(user, error)
		: GetHome(error);
}

#endif

AllocatedPath
ParsePath(const char *path, Error &error)
{
	assert(path != nullptr);

#ifndef WIN32
	if (path[0] == '~') {
		++path;

		if (*path == '\0')
			return GetConfiguredHome(error);

		AllocatedPath home = AllocatedPath::Null();

		if (*path == '/') {
			home = GetConfiguredHome(error);

			++path;
		} else {
			const char *slash = strchr(path, '/');
			const char *end = slash == nullptr
					? path + strlen(path)
					: slash;
			const std::string user(path, end);
			home = GetHome(user.c_str(), error);

			if (slash == nullptr)
				return home;

			path = slash + 1;
		}

		if (home.IsNull())
			return AllocatedPath::Null();

		AllocatedPath path2 = AllocatedPath::FromUTF8(path, error);
		if (path2.IsNull())
			return AllocatedPath::Null();

		return AllocatedPath::Build(home, path2);
	} else if (!PathTraitsUTF8::IsAbsolute(path)) {
		error.Format(path_domain,
			     "not an absolute path: %s", path);
		return AllocatedPath::Null();
	} else {
#endif
		return AllocatedPath::FromUTF8(path, error);
#ifndef WIN32
	}
#endif
}
