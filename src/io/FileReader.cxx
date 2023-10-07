// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "FileReader.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileInfo.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/Open.hxx"

#include <cassert>

#ifdef _WIN32

FileReader::FileReader(Path path)
	:handle(CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
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

	return FileInfo{handle};
}

std::size_t
FileReader::Read(std::span<std::byte> dest)
{
	assert(IsDefined());

	DWORD nbytes;
	if (!ReadFile(handle, dest.data(), dest.size(), &nbytes, nullptr))
		throw MakeLastError("Failed to read from file");

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

FileReader::FileReader(Path path)
	:fd(OpenReadOnly(path.c_str()))
{
}

FileInfo
FileReader::GetFileInfo() const
{
	assert(IsDefined());

	return FileInfo{fd};
}

std::size_t
FileReader::Read(std::span<std::byte> dest)
{
	assert(IsDefined());

	ssize_t nbytes = fd.Read(dest);
	if (nbytes < 0)
		throw MakeErrno("Failed to read from file");

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
