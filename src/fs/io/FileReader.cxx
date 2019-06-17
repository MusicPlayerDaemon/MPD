/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "FileReader.hxx"
#include "fs/FileInfo.hxx"
#include "system/Error.hxx"
#include "system/Open.hxx"

#include <assert.h>

#ifdef _WIN32

FileReader::FileReader(Path _path)
	:path(_path),
	 handle(CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
			   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
			   nullptr))
{
	if (handle == INVALID_HANDLE_VALUE)
		throw FormatLastError("Failed to open %s", path.ToUTF8().c_str());
}

FileInfo
FileReader::GetFileInfo() const
{
	assert(IsDefined());

	return FileInfo(path);
}

size_t
FileReader::Read(void *data, size_t size)
{
	assert(IsDefined());

	DWORD nbytes;
	if (!ReadFile(handle, data, size, &nbytes, nullptr))
		throw FormatLastError("Failed to read from %s",
				      path.ToUTF8().c_str());

	return nbytes;
}

void
FileReader::Seek(off_t offset)
{
	assert(IsDefined());

	auto result = SetFilePointer(handle, offset, nullptr, FILE_BEGIN);
	if (result == INVALID_SET_FILE_POINTER)
		throw MakeLastError("Failed to seek");
}

void
FileReader::Skip(off_t offset)
{
	assert(IsDefined());

	auto result = SetFilePointer(handle, offset, nullptr, FILE_CURRENT);
	if (result == INVALID_SET_FILE_POINTER)
		throw MakeLastError("Failed to seek");
}

void
FileReader::Close() noexcept
{
	assert(IsDefined());

	CloseHandle(handle);
}

#else

FileReader::FileReader(Path _path)
	:path(_path), fd(OpenReadOnly(path.c_str()))
{
}

FileInfo
FileReader::GetFileInfo() const
{
	assert(IsDefined());

	FileInfo info;
	const bool success = fstat(fd.Get(), &info.st) == 0;
	if (!success)
		throw FormatErrno("Failed to access %s",
				  path.ToUTF8().c_str());

	return info;
}

size_t
FileReader::Read(void *data, size_t size)
{
	assert(IsDefined());

	ssize_t nbytes = fd.Read(data, size);
	if (nbytes < 0)
		throw FormatErrno("Failed to read from %s", path.ToUTF8().c_str());

	return nbytes;
}

void
FileReader::Seek(off_t offset)
{
	assert(IsDefined());

	auto result = fd.Seek(offset);
	const bool success = result >= 0;
	if (!success)
		throw MakeErrno("Failed to seek");
}

void
FileReader::Skip(off_t offset)
{
	assert(IsDefined());

	auto result = fd.Skip(offset);
	const bool success = result >= 0;
	if (!success)
		throw MakeErrno("Failed to seek");
}

void
FileReader::Close() noexcept
{
	assert(IsDefined());

	fd.Close();
}

#endif
