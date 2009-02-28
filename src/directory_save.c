/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "directory_save.h"
#include "directory.h"
#include "song.h"
#include "log.h"
#include "path.h"
#include "song_save.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#define DIRECTORY_MTIME "mtime: "
#define DIRECTORY_BEGIN "begin: "
#define DIRECTORY_END "end: "

/* TODO error checking */
int
directory_save(FILE *fp, struct directory *directory)
{
	struct dirvec *children = &directory->children;
	size_t i;
	int retv;

	if (!directory_is_root(directory)) {
		fprintf(fp, DIRECTORY_MTIME "%lu\n",
			(unsigned long)directory->mtime);

		retv = fprintf(fp, "%s%s\n", DIRECTORY_BEGIN,
			       directory_get_path(directory));
		if (retv < 0)
			return -1;
	}

	for (i = 0; i < children->nr; ++i) {
		struct directory *cur = children->base[i];
		char *base = g_path_get_basename(cur->path);

		retv = fprintf(fp, DIRECTORY_DIR "%s\n", base);
		g_free(base);
		if (retv < 0)
			return -1;
		if (directory_save(fp, cur) < 0)
			return -1;
	}

	songvec_save(fp, &directory->songs);

	if (!directory_is_root(directory) &&
	    fprintf(fp, DIRECTORY_END "%s\n",
		    directory_get_path(directory)) < 0)
		return -1;
	return 0;
}

void
directory_load(FILE *fp, struct directory *directory)
{
	char buffer[MPD_PATH_MAX * 2];
	char key[MPD_PATH_MAX * 2];
	char *name;

	while (fgets(buffer, sizeof(buffer), fp)
	       && !g_str_has_prefix(buffer, DIRECTORY_END)) {
		if (g_str_has_prefix(buffer, DIRECTORY_DIR)) {
			struct directory *subdir;

			g_strchomp(buffer);
			strcpy(key, &(buffer[strlen(DIRECTORY_DIR)]));
			if (!fgets(buffer, sizeof(buffer), fp))
				FATAL("Error reading db, fgets\n");

			if (g_str_has_prefix(buffer, DIRECTORY_MTIME)) {
				directory->mtime =
					g_ascii_strtoull(buffer + sizeof(DIRECTORY_MTIME) - 1,
							 NULL, 10);

				if (!fgets(buffer, sizeof(buffer), fp))
					FATAL("Error reading db, fgets\n");
			}

			if (!g_str_has_prefix(buffer, DIRECTORY_BEGIN))
				FATAL("Error reading db at line: %s\n", buffer);
			g_strchomp(buffer);
			name = &(buffer[strlen(DIRECTORY_BEGIN)]);
			if (!g_str_has_prefix(name, directory->path) != 0)
				FATAL("Wrong path in database: '%s' in '%s'\n",
				      name, directory->path);

			subdir = directory_get_child(directory, name);
			if (subdir != NULL) {
				assert(subdir->parent == directory);
			} else {
				subdir = directory_new(name, directory);
				dirvec_add(&directory->children, subdir);
			}
			directory_load(fp, subdir);
		} else if (g_str_has_prefix(buffer, SONG_BEGIN)) {
			readSongInfoIntoList(fp, &directory->songs, directory);
		} else {
			FATAL("Unknown line in db: %s\n", buffer);
		}
	}
}
