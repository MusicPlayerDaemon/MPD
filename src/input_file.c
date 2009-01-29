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

#include "input_file.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>

static bool
input_file_open(struct input_stream *is, const char *filename)
{
	int fd, ret;
	struct stat st;

	if (filename[0] != '/')
		return false;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		is->error = errno;
		return false;
	}

	is->seekable = true;

	ret = fstat(fd, &st);
	if (ret < 0) {
		is->error = errno;
		close(fd);
		return false;
	}

	if (!S_ISREG(st.st_mode)) {
		g_debug("Not a regular file: %s\n", filename);
		is->error = EINVAL;
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
input_file_seek(struct input_stream *is, off_t offset, int whence)
{
	int fd = GPOINTER_TO_INT(is->data);

	offset = lseek(fd, offset, whence);
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
	.open = input_file_open,
	.close = input_file_close,
	.read = input_file_read,
	.eof = input_file_eof,
	.seek = input_file_seek,
};
