/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "config.h"
#include "FileOutputStream.hxx"
#include "system/Error.hxx"

#ifdef WIN32

FileOutputStream::FileOutputStream(Path _path)
	:BaseFileOutputStream(_path)
{
	SetHandle(CreateFile(_path.c_str(), GENERIC_WRITE, 0, nullptr,
			     CREATE_ALWAYS,
			     FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,
			     nullptr));
	if (!IsDefined())
		throw FormatLastError("Failed to create %s",
				      GetPath().ToUTF8().c_str());
}

uint64_t
BaseFileOutputStream::Tell() const
{
	LONG high = 0;
	DWORD low = SetFilePointer(handle, 0, &high, FILE_CURRENT);
	if (low == 0xffffffff)
		return 0;

	return uint64_t(high) << 32 | uint64_t(low);
}

void
BaseFileOutputStream::Write(const void *data, size_t size)
{
	assert(IsDefined());

	DWORD nbytes;
	if (!WriteFile(handle, data, size, &nbytes, nullptr))
		throw FormatLastError("Failed to write to %s",
				      GetPath().c_str());

	if (size_t(nbytes) != size)
		throw FormatLastError(ERROR_DISK_FULL, "Failed to write to %s",
				      GetPath().c_str());
}

void
FileOutputStream::Commit()
{
	assert(IsDefined());

	Close();
}

void
FileOutputStream::Cancel()
{
	assert(IsDefined());

	Close();

	DeleteFile(GetPath().c_str());
}

#else

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#ifdef HAVE_LINKAT
#ifndef O_TMPFILE
/* supported since Linux 3.11 */
#define __O_TMPFILE 020000000
#define O_TMPFILE (__O_TMPFILE | O_DIRECTORY)
#include <stdio.h>
#endif

/**
 * Open a file using Linux's O_TMPFILE for writing the given file.
 */
static bool
OpenTempFile(FileDescriptor &fd, Path path)
{
	const auto directory = path.GetDirectoryName();
	if (directory.IsNull())
		return false;

	return fd.Open(directory.c_str(), O_TMPFILE|O_WRONLY, 0666);
}

#endif /* HAVE_LINKAT */

FileOutputStream::FileOutputStream(Path _path)
	:BaseFileOutputStream(_path)
{
#ifdef HAVE_LINKAT
	/* try Linux's O_TMPFILE first */
	is_tmpfile = OpenTempFile(SetFD(), GetPath());
	if (!is_tmpfile) {
#endif
		/* fall back to plain POSIX */
		if (!SetFD().Open(GetPath().c_str(),
				  O_WRONLY|O_CREAT|O_TRUNC,
				  0666))
			throw FormatErrno("Failed to create %s",
					  GetPath().c_str());
#ifdef HAVE_LINKAT
	}
#endif
}

uint64_t
BaseFileOutputStream::Tell() const
{
	return fd.Tell();
}

void
BaseFileOutputStream::Write(const void *data, size_t size)
{
	assert(IsDefined());

	ssize_t nbytes = fd.Write(data, size);
	if (nbytes < 0)
		throw FormatErrno("Failed to write to %s", GetPath().c_str());
	else if ((size_t)nbytes < size)
		throw FormatErrno(ENOSPC, "Failed to write to %s",
				  GetPath().c_str());
}

void
FileOutputStream::Commit()
{
	assert(IsDefined());

#ifdef HAVE_LINKAT
	if (is_tmpfile) {
		unlink(GetPath().c_str());

		/* hard-link the temporary file to the final path */
		char fd_path[64];
		snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d",
			 GetFD().Get());
		if (linkat(AT_FDCWD, fd_path, AT_FDCWD, GetPath().c_str(),
			   AT_SYMLINK_FOLLOW) < 0)
			throw FormatErrno("Failed to commit %s",
					  GetPath().c_str());
	}
#endif

	if (!Close()) {
#ifdef WIN32
		throw FormatLastError("Failed to commit %s",
				      GetPath().ToUTF8().c_str());
#else
		throw FormatErrno("Failed to commit %s", GetPath().c_str());
#endif
	}
}

void
FileOutputStream::Cancel()
{
	assert(IsDefined());

	Close();

#ifdef HAVE_LINKAT
	if (!is_tmpfile)
#endif
		unlink(GetPath().c_str());
}

#endif

AppendFileOutputStream::AppendFileOutputStream(Path _path)
	:BaseFileOutputStream(_path)
{
#ifdef WIN32
	SetHandle(CreateFile(GetPath().c_str(), GENERIC_WRITE, 0, nullptr,
			     OPEN_EXISTING,
			     FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,
			     nullptr));
	if (!IsDefined())
		throw FormatLastError("Failed to append to %s",
				      GetPath().ToUTF8().c_str());

	if (!SeekEOF()) {
		auto code = GetLastError();
		Close();
		throw FormatLastError(code, "Failed seek end-of-file of %s",
				      GetPath().ToUTF8().c_str());
	}
#else
	if (!SetFD().Open(GetPath().c_str(),
			  O_WRONLY|O_APPEND))
		throw FormatErrno("Failed to append to %s",
				  GetPath().c_str());
#endif
}

void
AppendFileOutputStream::Commit()
{
	assert(IsDefined());

	if (!Close()) {
#ifdef WIN32
		throw FormatLastError("Failed to commit %s",
				      GetPath().ToUTF8().c_str());
#else
		throw FormatErrno("Failed to commit %s", GetPath().c_str());
#endif
	}
}
