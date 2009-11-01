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

#include "directory_save.h"
#include "directory.h"
#include "song.h"
#include "path.h"
#include "song_save.h"

#include <assert.h>
#include <string.h>

#define DIRECTORY_MTIME "mtime: "
#define DIRECTORY_BEGIN "begin: "
#define DIRECTORY_END "end: "

/**
 * The quark used for GError.domain.
 */
static inline GQuark
directory_quark(void)
{
	return g_quark_from_static_string("directory");
}

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

static struct directory *
directory_load_subdir(FILE *fp, struct directory *parent, const char *name,
		      GError **error_r)
{
	char buffer[MPD_PATH_MAX * 2];
	struct directory *directory;
	bool success;

	if (!fgets(buffer, sizeof(buffer), fp)) {
		g_set_error(error_r, directory_quark(), 0,
			    "Unexpected end of file");
		return NULL;
	}

	if (g_str_has_prefix(buffer, DIRECTORY_MTIME)) {
		parent->mtime =
			g_ascii_strtoull(buffer + sizeof(DIRECTORY_MTIME) - 1,
					 NULL, 10);

		if (!fgets(buffer, sizeof(buffer), fp)) {
			g_set_error(error_r, directory_quark(), 0,
				    "Unexpected end of file");
			return NULL;
		}
	}

	if (!g_str_has_prefix(buffer, DIRECTORY_BEGIN)) {
		g_set_error(error_r, directory_quark(), 0,
			    "Malformed line: %s", buffer);
		return NULL;
	}

	g_strchomp(buffer);
	name = &(buffer[strlen(DIRECTORY_BEGIN)]);
	if (!g_str_has_prefix(name, parent->path) != 0) {
		g_set_error(error_r, directory_quark(), 0,
			    "Wrong path in database: '%s' in '%s'",
			    name, parent->path);
		return NULL;
	}

	directory = directory_get_child(parent, name);
	if (directory != NULL) {
		assert(directory->parent == parent);
	} else {
		directory = directory_new(name, parent);
		dirvec_add(&parent->children, directory);
	}

	success = directory_load(fp, directory, error_r);
	if (!success)
		return NULL;

	return directory;
}

bool
directory_load(FILE *fp, struct directory *directory, GError **error)
{
	char buffer[MPD_PATH_MAX * 2];
	bool success;

	while (fgets(buffer, sizeof(buffer), fp)
	       && !g_str_has_prefix(buffer, DIRECTORY_END)) {
		g_strchomp(buffer);

		if (g_str_has_prefix(buffer, DIRECTORY_DIR)) {
			struct directory *subdir =
				directory_load_subdir(fp, directory,
						      buffer + sizeof(DIRECTORY_DIR) - 1,
						      error);
			if (subdir == NULL)
				return false;
		} else if (g_str_has_prefix(buffer, SONG_BEGIN)) {
			success = songvec_load(fp, &directory->songs,
					       directory, error);
			if (!success)
				return false;
		} else {
			g_set_error(error, directory_quark(), 0,
				    "Malformed line: %s", buffer);
			return false;
		}
	}

	return true;
}
