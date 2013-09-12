/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "fs/Path.hxx"
#include "util/Error.hxx"
#include "ConfigGlobal.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

#ifndef WIN32
#include <pwd.h>

/**
 * Determine a given user's home directory.
 */
static Path
GetHome(const char *user, Error &error)
{
	passwd *pw = getpwnam(user);
	if (pw == nullptr) {
		error.Format(path_domain,
			     "no such user: %s", user);
		return Path::Null();
	}

	return Path::FromFS(pw->pw_dir);
}

/**
 * Determine the current user's home directory.
 */
static Path
GetHome(Error &error)
{
	const char *home = g_get_home_dir();
	if (home == nullptr) {
		error.Set(path_domain,
			  "problems getting home for current user");
		return Path::Null();
	}

	return Path::FromUTF8(home, error);
}

/**
 * Determine the configured user's home directory.
 */
static Path
GetConfiguredHome(Error &error)
{
	const char *user = config_get_string(CONF_USER, nullptr);
	return user != nullptr
		? GetHome(user, error)
		: GetHome(error);
}

#endif

Path
ParsePath(const char *path, Error &error)
{
	assert(path != nullptr);

	Path path2 = Path::FromUTF8(path, error);
	if (path2.IsNull())
		return Path::Null();

#ifndef WIN32
	if (!g_path_is_absolute(path) && path[0] != '~') {
		error.Format(path_domain,
			     "not an absolute path: %s", path);
		return Path::Null();
	} else if (path[0] == '~') {
		Path home = Path::Null();

		if (path[1] == '/' || path[1] == '\0') {
			home = GetConfiguredHome(error);

			++path;
		} else {
			++path;

			const char *slash = strchr(path, '/');
			char *user = slash != nullptr
				? g_strndup(path, slash - path)
				: g_strdup(path);

			home = GetHome(user, error);
			g_free(user);

			path = slash;
		}

		if (home.IsNull())
			return Path::Null();

		return Path::Build(home, path2);
	} else {
#endif
		return path2;
#ifndef WIN32
	}
#endif
}
