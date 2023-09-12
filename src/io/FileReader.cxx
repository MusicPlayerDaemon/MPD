// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "FileReader.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "fs/FileInfo.hxx"
#include "lib/fmt/SystemError.hxx"
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
		throw FmtLastError("Failed to open {}", path);
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
		throw FmtLastError("Failed to read from {}", path);

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
		throw FmtErrno("Failed to access {}", path);

	return info;
}

std::size_t
FileReader::Read(void *data, std::size_t size)
{
	assert(IsDefined());

	ssize_t nbytes = fd.Read(data, size);
	if (nbytes < 0)
		throw FmtErrno("Failed to read from {}", path);

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

#endif
