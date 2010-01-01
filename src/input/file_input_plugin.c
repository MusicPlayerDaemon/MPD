/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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

#include "config.h" /* must be first for large file support */
#include "input/file_input_plugin.h"
#include "input_plugin.h"
#include "fd_util.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_file"

static inline GQuark
file_quark(void)
{
	return g_quark_from_static_string("file");
}

static bool
input_file_open(struct input_stream *is, const char *filename,
		GError **error_r)
{
	int fd, ret;
	struct stat st;

	if (!g_path_is_absolute(filename))
		return false;

	fd = open_cloexec(filename, O_RDONLY, 0);
	if (fd < 0) {
		if (errno != ENOENT && errno != ENOTDIR)
			g_set_error(error_r, file_quark(), errno,
				    "Failed to open \"%s\": %s",
				    filename, g_strerror(errno));
		return false;
	}

	is->seekable = true;

	ret = fstat(fd, &st);
	if (ret < 0) {
		g_set_error(error_r, file_quark(), errno,
			    "Failed to stat \"%s\": %s",
			    filename, g_strerror(errno));
		close(fd);
		return false;
	}

	if (!S_ISREG(st.st_mode)) {
		g_set_error(error_r, file_quark(), 0,
			    "Not a regular file: %s", filename);
		close(fd);
		return false;
	}

	is->size = st.st_size;

#ifdef POSIX_FADV_SEQUENTIAL
	posix_fadvise(fd, (off_t)0, is->size, POSIX_FADV_SEQUENTIAL);
#endif

	is->plugin = &input_plugin_file;
	is->data = GINT_TO_POINTER(fd);
	is->ready = true;

	return true;
}

static bool
input_file_seek(struct input_stream *is, goffset offset, int whence,
		GError **error_r)
{
	int fd = GPOINTER_TO_INT(is->data);

	offset = (goffset)lseek(fd, (off_t)offset, whence);
	if (offset < 0) {
		g_set_error(error_r, file_quark(), errno,
			    "Failed to seek: %s", g_strerror(errno));
		return false;
	}

	is->offset = offset;
	return true;
}

static size_t
input_file_read(struct input_stream *is, void *ptr, size_t size,
		GError **error_r)
{
	int fd = GPOINTER_TO_INT(is->data);
	ssize_t nbytes;

	nbytes = read(fd, ptr, size);
	if (nbytes < 0) {
		g_set_error(error_r, file_quark(), errno,
			    "Failed to read: %s", g_strerror(errno));
		return 0;
	}

	is->offset += nbytes;
	return (size_t)nbytes;
}

static void
input_file_close(struct input_stream *is)
{
	int fd = GPOINTER_TO_INT(is->data);

	close(fd);
}

static bool
input_file_eof(struct input_stream *is)
{
	return is->offset >= is->size;
}

const struct input_plugin input_plugin_file = {
	.name = "file",
	.open = input_file_open,
	.close = input_file_close,
	.read = input_file_read,
	.eof = input_file_eof,
	.seek = input_file_seek,
};
