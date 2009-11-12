/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "utils.h"
#include "conf.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#ifndef WIN32
#include <pwd.h>
#endif

#ifdef HAVE_IPV6
#include <sys/socket.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

char *parsePath(char *path)
{
#ifndef WIN32
	if (!g_path_is_absolute(path) && path[0] != '~') {
		g_warning("\"%s\" is not an absolute path", path);
		return NULL;
	} else if (path[0] == '~') {
		size_t pos = 1;
		const char *home;

		if (path[1] == '/' || path[1] == '\0') {
			const char *user = config_get_string(CONF_USER, NULL);
			if (user != NULL) {
				struct passwd *passwd = getpwnam(user);
				if (!passwd) {
					g_warning("no such user %s", user);
					return NULL;
				}

				home = passwd->pw_dir;
			} else {
				home = g_get_home_dir();
				if (home == NULL) {
					g_warning("problems getting home "
						  "for current user");
					return NULL;
				}
			}
		} else {
			bool foundSlash = false;
			struct passwd *passwd;
			char *c;

			for (c = path + 1; *c != '\0' && *c != '/'; c++);
			if (*c == '/') {
				foundSlash = true;
				*c = '\0';
			}
			pos = c - path;

			passwd = getpwnam(path + 1);
			if (!passwd) {
				g_warning("user \"%s\" not found", path + 1);
				return NULL;
			}

			if (foundSlash)
				*c = '/';

			home = passwd->pw_dir;
		}

		return g_strconcat(home, path + pos, NULL);
	} else {
#endif
		return g_strdup(path);
#ifndef WIN32
	}
#endif
}

bool
string_array_contains(const char *const* haystack, const char *needle)
{
	assert(haystack != NULL);
	assert(needle != NULL);

	for (; *haystack != NULL; ++haystack)
		if (g_ascii_strcasecmp(*haystack, needle) == 0)
			return true;

	return false;
}
