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

#include "archive_plugin.h"
#include "archive_internal.h"

#include <assert.h>

struct archive_file *
archive_file_open(const struct archive_plugin *plugin, const char *path,
		  GError **error_r)
{
	struct archive_file *file;

	assert(plugin != NULL);
	assert(plugin->open != NULL);
	assert(path != NULL);
	assert(error_r == NULL || *error_r == NULL);

	file = plugin->open(path, error_r);

	if (file != NULL) {
		assert(file->plugin != NULL);
		assert(file->plugin->close != NULL);
		assert(file->plugin->scan_reset != NULL);
		assert(file->plugin->scan_next != NULL);
		assert(file->plugin->open_stream != NULL);
		assert(error_r == NULL || *error_r == NULL);
	} else {
		assert(error_r == NULL || *error_r != NULL);
	}

	return file;
}

void
archive_file_close(struct archive_file *file)
{
	assert(file != NULL);
	assert(file->plugin != NULL);
	assert(file->plugin->close != NULL);

	file->plugin->close(file);
}

void
archive_file_scan_reset(struct archive_file *file)
{
	assert(file != NULL);
	assert(file->plugin != NULL);
	assert(file->plugin->scan_reset != NULL);
	assert(file->plugin->scan_next != NULL);

	file->plugin->scan_reset(file);
}

char *
archive_file_scan_next(struct archive_file *file)
{
	assert(file != NULL);
	assert(file->plugin != NULL);
	assert(file->plugin->scan_next != NULL);

	return file->plugin->scan_next(file);
}

struct input_stream *
archive_file_open_stream(struct archive_file *file, const char *path,
			 GMutex *mutex, GCond *cond,
			 GError **error_r)
{
	assert(file != NULL);
	assert(file->plugin != NULL);
	assert(file->plugin->open_stream != NULL);

	return file->plugin->open_stream(file, path, mutex, cond,
					 error_r);
}
