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

#include "archive_plugin.h"
#include "archive_internal.h"

#include <assert.h>

struct archive_file *
archive_file_open(const struct archive_plugin *plugin, const char *path)
{
	struct archive_file *file;

	assert(plugin != NULL);
	assert(plugin->open != NULL);
	assert(path != NULL);

	file = plugin->open(path);

	if (file != NULL) {
		assert(file->plugin != NULL);
		assert(file->plugin->close != NULL);
		assert(file->plugin->scan_reset != NULL);
		assert(file->plugin->scan_next != NULL);
		assert(file->plugin->open_stream != NULL);
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

bool
archive_file_open_stream(struct archive_file *file, struct input_stream *is,
			 const char *path, GError **error_r)
{
	assert(file != NULL);
	assert(file->plugin != NULL);
	assert(file->plugin->open_stream != NULL);

	return file->plugin->open_stream(file, is, path, error_r);
}
