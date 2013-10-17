/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "FileInputPlugin.hxx"
#include "InputInternal.hxx"
#include "InputStream.hxx"
#include "InputPlugin.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "fs/Path.hxx"
#include "system/fd_util.h"
#include "open.h"

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>

static constexpr Domain file_domain("file");

struct FileInputStream {
	struct input_stream base;

	int fd;

	FileInputStream(const char *path, int _fd, off_t size,
			Mutex &mutex, Cond &cond)
		:base(input_plugin_file, path, mutex, cond),
		 fd(_fd) {
		base.size = size;
		base.seekable = true;
		base.ready = true;
	}

	~FileInputStream() {
		close(fd);
	}
};

static struct input_stream *
input_file_open(const char *filename,
		Mutex &mutex, Cond &cond,
		Error &error)
{
	int fd, ret;
	struct stat st;

	if (!Path::IsAbsoluteFS(filename))
		return nullptr;

	fd = open_cloexec(filename, O_RDONLY|O_BINARY, 0);
	if (fd < 0) {
		if (errno != ENOENT && errno != ENOTDIR)
			error.FormatErrno("Failed to open \"%s\"",
					  filename);
		return nullptr;
	}

	ret = fstat(fd, &st);
	if (ret < 0) {
		error.FormatErrno("Failed to stat \"%s\"", filename);
		close(fd);
		return nullptr;
	}

	if (!S_ISREG(st.st_mode)) {
		error.Format(file_domain, "Not a regular file: %s", filename);
		close(fd);
		return nullptr;
	}

#ifdef POSIX_FADV_SEQUENTIAL
	posix_fadvise(fd, (off_t)0, st.st_size, POSIX_FADV_SEQUENTIAL);
#endif

	FileInputStream *fis = new FileInputStream(filename, fd, st.st_size,
						   mutex, cond);
	return &fis->base;
}

static bool
input_file_seek(struct input_stream *is, goffset offset, int whence,
		Error &error)
{
	FileInputStream *fis = (FileInputStream *)is;

	offset = (goffset)lseek(fis->fd, (off_t)offset, whence);
	if (offset < 0) {
		error.SetErrno("Failed to seek");
		return false;
	}

	is->offset = offset;
	return true;
}

static size_t
input_file_read(struct input_stream *is, void *ptr, size_t size,
		Error &error)
{
	FileInputStream *fis = (FileInputStream *)is;
	ssize_t nbytes;

	nbytes = read(fis->fd, ptr, size);
	if (nbytes < 0) {
		error.SetErrno("Failed to read");
		return 0;
	}

	is->offset += nbytes;
	return (size_t)nbytes;
}

static void
input_file_close(struct input_stream *is)
{
	FileInputStream *fis = (FileInputStream *)is;

	delete fis;
}

static bool
input_file_eof(struct input_stream *is)
{
	return is->offset >= is->size;
}

const InputPlugin input_plugin_file = {
	"file",
	nullptr,
	nullptr,
	input_file_open,
	input_file_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	input_file_read,
	input_file_eof,
	input_file_seek,
};
