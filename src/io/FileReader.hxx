// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef FILE_READER_HXX
#define FILE_READER_HXX

#include "Reader.hxx"
#include "fs/AllocatedPath.hxx"

#ifdef _WIN32
#include <fileapi.h>
#include <handleapi.h> // for INVALID_HANDLE_VALUE
#include <windef.h> // for HWND (needed by winbase.h)
#include <winbase.h> // for FILE_CURRENT
#else
#include "io/UniqueFileDescriptor.hxx"
#endif

#include <cstdint>

class Path;
class FileInfo;

class FileReader final : public Reader {
	AllocatedPath path;

#ifdef _WIN32
	HANDLE handle;
#else
	UniqueFileDescriptor fd;
#endif

public:
	explicit FileReader(Path _path);

#ifdef _WIN32
	FileReader(FileReader &&other) noexcept
		:path(std::move(other.path)),
		 handle(std::exchange(other.handle, INVALID_HANDLE_VALUE)) {}

	~FileReader() noexcept {
		if (IsDefined())
			Close();
	}
#else
	FileReader(FileReader &&other) noexcept
		:path(std::move(other.path)),
		 fd(std::move(other.fd)) {}
#endif


protected:
	bool IsDefined() const noexcept {
#ifdef _WIN32
		return handle != INVALID_HANDLE_VALUE;
#else
		return fd.IsDefined();
#endif
	}

public:
#ifndef _WIN32
	FileDescriptor GetFD() const noexcept {
		return fd;
	}
#endif

	void Close() noexcept;

	FileInfo GetFileInfo() const;

	[[gnu::pure]]
	uint64_t GetSize() const noexcept {
#ifdef _WIN32
		LARGE_INTEGER size;
		return GetFileSizeEx(handle, &size)
			? size.QuadPart
			: 0;
#else
		return fd.GetSize();
#endif
	}

	[[gnu::pure]]
	uint64_t GetPosition() const noexcept {
#ifdef _WIN32
		LARGE_INTEGER zero;
		zero.QuadPart = 0;
		LARGE_INTEGER position;
		return SetFilePointerEx(handle, zero, &position, FILE_CURRENT)
			? position.QuadPart
			: 0;
#else
		return fd.Tell();
#endif
	}

	void Rewind() {
		Seek(0);
	}

	void Seek(off_t offset);
	void Skip(off_t offset);

	/* virtual methods from class Reader */
	std::size_t Read(void *data, std::size_t size) override;
};

#endif
