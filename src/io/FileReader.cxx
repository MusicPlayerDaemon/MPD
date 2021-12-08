/*
 * Copyright 2014-2021 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "FileReader.hxx"
#include "fs/FileInfo.hxx"
#include "system/Error.hxx"
#include "io/Open.hxx"

#include <cassert>

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

std::size_t
FileReader::Read(void *data, std::size_t size)
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

std::size_t
FileReader::Read(void *data, std::size_t size)
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
