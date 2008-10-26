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

#include "log.h"
#include "os_compat.h"

static bool
input_file_open(struct input_stream *is, const char *filename)
{
	FILE *fp;

	fp = fopen(filename, "r");
	if (!fp) {
		is->error = errno;
		return false;
	}

	is->seekable = 1;

	fseek(fp, 0, SEEK_END);
	is->size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

#ifdef POSIX_FADV_SEQUENTIAL
	posix_fadvise(fileno(fp), (off_t)0, is->size, POSIX_FADV_SEQUENTIAL);
#endif

	is->data = fp;
	is->ready = 1;

	return true;
}

static int
input_file_seek(struct input_stream *is, long offset, int whence)
{
	if (fseek((FILE *) is->data, offset, whence) == 0) {
		is->offset = ftell((FILE *) is->data);
	} else {
		is->error = errno;
		return -1;
	}

	return 0;
}

static size_t
input_file_read(struct input_stream *is, void *ptr, size_t size)
{
	size_t readSize;

	readSize = fread(ptr, 1, size, (FILE *) is->data);
	if (readSize <= 0 && ferror((FILE *) is->data)) {
		is->error = errno;
		DEBUG("input_file_read: error reading: %s\n",
		      strerror(is->error));
	}

	is->offset = ftell((FILE *) is->data);

	return readSize;
}

static int
input_file_close(struct input_stream *is)
{
	if (fclose((FILE *) is->data) < 0) {
		is->error = errno;
		return -1;
	}

	return 0;
}

static int
input_file_eof(struct input_stream *is)
{
	if (feof((FILE *) is->data))
		return 1;

	if (ferror((FILE *) is->data) && is->error != EINTR) {
		return 1;
	}

	return 0;
}

static int
input_file_buffer(mpd_unused struct input_stream *is)
{
	return 0;
}

const struct input_plugin input_plugin_file = {
	.open = input_file_open,
	.close = input_file_close,
	.buffer = input_file_buffer,
	.read = input_file_read,
	.eof = input_file_eof,
	.seek = input_file_seek,
};
