/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
 * The .mpdignore backend code.
 *
 */

#include "config.h"
#include "ExcludeList.hxx"
#include "fs/Path.hxx"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

bool
ExcludeList::LoadFile(const Path &path_fs)
{
	FILE *file = fopen(path_fs.c_str(), "r");
	if (file == NULL) {
		if (errno != ENOENT) {
			char *path_utf8 = path_fs.ToUTF8();
			g_debug("Failed to open %s: %s",
				path_utf8, g_strerror(errno));
			g_free(path_utf8);
		}

		return false;
	}

	char line[1024];
	while (fgets(line, sizeof(line), file) != NULL) {
		char *p = strchr(line, '#');
		if (p != NULL)
			*p = 0;

		p = g_strstrip(line);
		if (*p != 0)
			patterns.emplace_front(p);
	}

	fclose(file);

	return true;
}

bool
ExcludeList::Check(const char *name_fs) const
{
	assert(name_fs != NULL);

	/* XXX include full path name in check */

	for (const auto &i : patterns)
		if (i.Check(name_fs))
			return true;

	return false;
}
