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

static bool
input_file_open(struct input_stream *is, const char *filename)
{
	int fd, ret;
	struct stat st;

	char* pathname = g_strdup(filename);

	if (!g_path_is_absolute(filename))
	{
		g_free(pathname);
		return false;
	}

	if (stat(filename, &st) < 0) {
		char* slash = strrchr(pathname, '/');
		*slash = '\0';
	}

	fd = open_cloexec(pathname, O_RDONLY, 0);
	if (fd < 0) {
		is->error = errno;
		g_debug("Failed to open \"%s\": %s",
			pathname, g_strerror(errno));
		g_free(pathname);
		return false;
	}

	is->seekable = true;

	ret = fstat(fd, &st);
	if (ret < 0) {
		is->error = errno;
		close(fd);
		g_free(pathname);
		return false;
	}

	if (!S_ISREG(st.st_mode)) {
		g_debug("Not a regular file: %s", pathname);
		is->error = EINVAL;
		close(fd);
		g_free(pathname);
		return false;
	}

	is->size = st.st_size;

#ifdef POSIX_FADV_SEQUENTIAL
	posix_fadvise(fd, (off_t)0, is->size, POSIX_FADV_SEQUENTIAL);
#endif

	is->plugin = &input_plugin_file;
	is->data = GINT_TO_POINTER(fd);
	is->ready = true;

	g_free(pathname);

	return true;
}

static bool
input_file_seek(struct input_stream *is, goffset offset, int whence)
{
	int fd = GPOINTER_TO_INT(is->data);

	offset = (goffset)lseek(fd, (off_t)offset, whence);
	if (offset < 0) {
		is->error = errno;
		return false;
	}

	is->offset = offset;
	return true;
}

static size_t
input_file_read(struct input_stream *is, void *ptr, size_t size)
{
	int fd = GPOINTER_TO_INT(is->data);
	ssize_t nbytes;

	nbytes = read(fd, ptr, size);
	if (nbytes < 0) {
		is->error = errno;
		g_debug("input_file_read: error reading: %s\n",
			strerror(is->error));
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
