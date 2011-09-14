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

#include "config.h" /* must be first for large file support */
#include "input/file_input_plugin.h"
#include "input_internal.h"
#include "input_plugin.h"
#include "fd_util.h"
#include "open.h"

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_file"

struct file_input_stream {
	struct input_stream base;

	int fd;
};

static inline GQuark
file_quark(void)
{
	return g_quark_from_static_string("file");
}

static struct input_stream *
input_file_open(const char *filename,
		GMutex *mutex, GCond *cond,
		GError **error_r)
{
	int fd, ret;
	struct stat st;
	struct file_input_stream *fis;

	if (!g_path_is_absolute(filename))
		return NULL;

	fd = open_cloexec(filename, O_RDONLY|O_BINARY, 0);
	if (fd < 0) {
		if (errno != ENOENT && errno != ENOTDIR)
			g_set_error(error_r, file_quark(), errno,
				    "Failed to open \"%s\": %s",
				    filename, g_strerror(errno));
		return NULL;
	}

	ret = fstat(fd, &st);
	if (ret < 0) {
		g_set_error(error_r, file_quark(), errno,
			    "Failed to stat \"%s\": %s",
			    filename, g_strerror(errno));
		close(fd);
		return NULL;
	}

	if (!S_ISREG(st.st_mode)) {
		g_set_error(error_r, file_quark(), 0,
			    "Not a regular file: %s", filename);
		close(fd);
		return NULL;
	}

#ifdef POSIX_FADV_SEQUENTIAL
	posix_fadvise(fd, (off_t)0, st.st_size, POSIX_FADV_SEQUENTIAL);
#endif

	fis = g_new(struct file_input_stream, 1);
	input_stream_init(&fis->base, &input_plugin_file, filename,
			  mutex, cond);

	fis->base.size = st.st_size;
	fis->base.seekable = true;
	fis->base.ready = true;

	fis->fd = fd;

	return &fis->base;
}

static bool
input_file_seek(struct input_stream *is, goffset offset, int whence,
		GError **error_r)
{
	struct file_input_stream *fis = (struct file_input_stream *)is;

	offset = (goffset)lseek(fis->fd, (off_t)offset, whence);
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
	struct file_input_stream *fis = (struct file_input_stream *)is;
	ssize_t nbytes;

	nbytes = read(fis->fd, ptr, size);
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
	struct file_input_stream *fis = (struct file_input_stream *)is;

	close(fis->fd);
	input_stream_deinit(&fis->base);
	g_free(fis);
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
