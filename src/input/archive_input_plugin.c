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

#include "config.h"
#include "input/archive_input_plugin.h"
#include "archive_api.h"
#include "archive_list.h"
#include "input_plugin.h"

#include <glib.h>

/**
 * select correct archive plugin to handle the input stream
 * may allow stacking of archive plugins. for example for handling
 * tar.gz a gzip handler opens file (through inputfile stream)
 * then it opens a tar handler and sets gzip inputstream as
 * parent_stream so tar plugin fetches file data from gzip
 * plugin and gzip fetches file from disk
 */
static struct input_stream *
input_archive_open(const char *pathname,
		   GMutex *mutex, GCond *cond,
		   GError **error_r)
{
	const struct archive_plugin *arplug;
	struct archive_file *file;
	char *archive, *filename, *suffix, *pname;
	struct input_stream *is;

	if (!g_path_is_absolute(pathname))
		return NULL;

	pname = g_strdup(pathname);
	// archive_lookup will modify pname when true is returned
	if (!archive_lookup(pname, &archive, &filename, &suffix)) {
		g_debug("not an archive, lookup %s failed\n", pname);
		g_free(pname);
		return NULL;
	}

	//check which archive plugin to use (by ext)
	arplug = archive_plugin_from_suffix(suffix);
	if (!arplug) {
		g_warning("can't handle archive %s\n",archive);
		g_free(pname);
		return NULL;
	}

	file = archive_file_open(arplug, archive, error_r);
	if (file == NULL)
		return NULL;

	//setup fileops
	is = archive_file_open_stream(file, filename, mutex, cond,
				      error_r);
	archive_file_close(file);
	g_free(pname);

	return is;
}

const struct input_plugin input_plugin_archive = {
	.name = "archive",
	.open = input_archive_open,
};
