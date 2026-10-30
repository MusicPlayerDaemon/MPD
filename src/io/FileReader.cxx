// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "FileReader.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileInfo.hxx"
#include "fs/Open.hxx"
#include "fs/Path.hxx"
#include "system/Error.hxx"

#include <cassert>

FileReader::FileReader(Path path)
	:handle(OpenReadOnly(path)) {}

#ifdef _WIN32

FileInfo
FileReader::GetFileInfo() const
{
	assert(IsDefined());

	return FileInfo{handle.Get()};
}

std::size_t
FileReader::Read(std::span<std::byte> dest)
{
	assert(IsDefined());

	DWORD nbytes;
	if (!ReadFile(handle.Get(), dest.data(), dest.size(), &nbytes, nullptr))
		throw MakeLastError("Failed to read from file");

	return nbytes;
}

void
FileReader::Seek(off_t offset)
{
	assert(IsDefined());

	auto result = SetFilePointer(handle.Get(), offset, nullptr, FILE_BEGIN);
	if (result == INVALID_SET_FILE_POINTER)
		throw MakeLastError("Failed to seek");
}

void
FileReader::Skip(off_t offset)
{
	assert(IsDefined());

	auto result = SetFilePointer(handle.Get(), offset, nullptr, FILE_CURRENT);
	if (result == INVALID_SET_FILE_POINTER)
		throw MakeLastError("Failed to seek");
}

#else

FileInfo
FileReader::GetFileInfo() const
{
	assert(IsDefined());

	return FileInfo{handle};
}

std::size_t
FileReader::Read(std::span<std::byte> dest)
{
	assert(IsDefined());

	ssize_t nbytes = handle.Read(dest);
	if (nbytes < 0)
		throw MakeErrno("Failed to read from file");

	return nbytes;
}

void
FileReader::Seek(off_t offset)
{
	assert(IsDefined());

	auto result = handle.Seek(offset);
	const bool success = result >= 0;
	if (!success)
		throw MakeErrno("Failed to seek");
}

void
FileReader::Skip(off_t offset)
{
	assert(IsDefined());

	auto result = handle.Skip(offset);
	const bool success = result >= 0;
	if (!success)
		throw MakeErrno("Failed to seek");
}

#endif
