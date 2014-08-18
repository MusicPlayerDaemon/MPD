/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "fs/Traits.hxx"
#include "system/fd_util.h"
#include "open.h"

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static constexpr Domain file_domain("file");

struct FileInputStream final : public InputStream {
	int fd;

	FileInputStream(const char *path, int _fd, off_t _size,
			Mutex &_mutex, Cond &_cond)
		:InputStream(path, _mutex, _cond),
		 fd(_fd) {
		size = _size;
		seekable = true;
		SetReady();
	}

	~FileInputStream() {
		close(fd);
	}

	/* virtual methods from InputStream */

	bool IsEOF() override {
		return GetOffset() >= GetSize();
	}

	size_t Read(void *ptr, size_t size, Error &error) override;
	bool Seek(offset_type offset, Error &error) override;
};

static InputStream *
input_file_open(const char *filename,
		Mutex &mutex, Cond &cond,
		Error &error)
{
	int fd, ret;
	struct stat st;

	if (!PathTraitsFS::IsAbsolute(filename))
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

	return new FileInputStream(filename, fd, st.st_size, mutex, cond);
}

bool
FileInputStream::Seek(offset_type new_offset, Error &error)
{
	auto result = lseek(fd, (off_t)new_offset, SEEK_SET);
	if (result < 0) {
		error.SetErrno("Failed to seek");
		return false;
	}

	offset = (offset_type)result;
	return true;
}

size_t
FileInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	ssize_t nbytes = read(fd, ptr, read_size);
	if (nbytes < 0) {
		error.SetErrno("Failed to read");
		return 0;
	}

	offset += nbytes;
	return (size_t)nbytes;
}

const InputPlugin input_plugin_file = {
	"file",
	nullptr,
	nullptr,
	input_file_open,
};
