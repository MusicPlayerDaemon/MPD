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

/*
 * The .mpdignore backend code.
 *
 */

#include "config.h"
#include "exclude.h"
#include "path.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

GSList *
exclude_list_load(const char *path_fs)
{
	FILE *file;
	char line[1024];
	GSList *list = NULL;

	assert(path_fs != NULL);

	file = fopen(path_fs, "r");
	if (file == NULL) {
		if (errno != ENOENT) {
			char *path_utf8 = fs_charset_to_utf8(path_fs);
			g_debug("Failed to open %s: %s",
				path_utf8, g_strerror(errno));
			g_free(path_utf8);
		}

		return NULL;
	}

	while (fgets(line, sizeof(line), file) != NULL) {
		char *p = strchr(line, '#');
		if (p != NULL)
			*p = 0;

		p = g_strstrip(line);
		if (*p != 0)
			list = g_slist_prepend(list, g_pattern_spec_new(p));
	}

	fclose(file);

	return list;
}

void
exclude_list_free(GSList *list)
{
	while (list != NULL) {
		GPatternSpec *pattern = list->data;
		g_pattern_spec_free(pattern);
		list = g_slist_remove(list, list->data);
	}
}

bool
exclude_list_check(GSList *list, const char *name_fs)
{
	assert(name_fs != NULL);

	/* XXX include full path name in check */

	for (; list != NULL; list = list->next) {
		GPatternSpec *pattern = list->data;

		if (g_pattern_match_string(pattern, name_fs))
			return true;
	}

	return false;
}
