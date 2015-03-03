/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "fs/Path.hxx"
#include "system/FileDescriptor.hxx"

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static constexpr Domain file_domain("file");

class FileInputStream final : public InputStream {
	FileDescriptor fd;

public:
	FileInputStream(const char *path, FileDescriptor _fd, off_t _size,
			Mutex &_mutex, Cond &_cond)
		:InputStream(path, _mutex, _cond),
		 fd(_fd) {
		size = _size;
		seekable = true;
		SetReady();
	}

	~FileInputStream() {
		fd.Close();
	}

	/* virtual methods from InputStream */

	bool IsEOF() override {
		return GetOffset() >= GetSize();
	}

	size_t Read(void *ptr, size_t size, Error &error) override;
	bool Seek(offset_type offset, Error &error) override;
};

InputStream *
OpenFileInputStream(Path path,
		    Mutex &mutex, Cond &cond,
		    Error &error)
{
	FileDescriptor fd;
	if (!fd.OpenReadOnly(path.c_str())) {
		error.FormatErrno("Failed to open \"%s\"",
				  path.c_str());
		return nullptr;
	}

	struct stat st;
	if (fstat(fd.Get(), &st) < 0) {
		error.FormatErrno("Failed to stat \"%s\"", path.c_str());
		fd.Close();
		return nullptr;
	}

	if (!S_ISREG(st.st_mode)) {
		error.Format(file_domain, "Not a regular file: %s",
			     path.c_str());
		fd.Close();
		return nullptr;
	}

#ifdef POSIX_FADV_SEQUENTIAL
	posix_fadvise(fd.Get(), (off_t)0, st.st_size, POSIX_FADV_SEQUENTIAL);
#endif

	return new FileInputStream(path.c_str(), fd, st.st_size, mutex, cond);
}

static InputStream *
input_file_open(gcc_unused const char *filename,
		gcc_unused Mutex &mutex, gcc_unused Cond &cond,
		gcc_unused Error &error)
{
	/* dummy method; use OpenFileInputStream() instead */

	return nullptr;
}

bool
FileInputStream::Seek(offset_type new_offset, Error &error)
{
	auto result = fd.Seek((off_t)new_offset);
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
	ssize_t nbytes = fd.Read(ptr, read_size);
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
