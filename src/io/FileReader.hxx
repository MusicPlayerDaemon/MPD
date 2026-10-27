// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Reader.hxx"
#include "io/FileHandle.hxx"

#ifdef _WIN32
#include <fileapi.h>
#include <windef.h> // for HWND (needed by winbase.h)
#include <winbase.h> // for FILE_CURRENT
#endif

#include <cstdint>
#include <utility> // for std::exchange()

#include <sys/types.h> // for off_t

class Path;
class FileInfo;

class FileReader final : public Reader {
	UniqueFileHandle handle;

public:
	explicit FileReader(Path _path);

	explicit FileReader(UniqueFileHandle &&_handle) noexcept
		:handle(std::move(_handle)) {}

	FileReader(FileReader &&other) noexcept
		:handle(std::move(other.handle)) {}

protected:
	bool IsDefined() const noexcept {
		return handle.IsDefined();
	}

public:
#ifndef _WIN32
	FileDescriptor GetFD() const noexcept {
		return handle;
	}
#endif

	FileInfo GetFileInfo() const;

	[[gnu::pure]]
	uint_least64_t GetSize() const noexcept {
#ifdef _WIN32
		LARGE_INTEGER size;
		return GetFileSizeEx(handle.Get(), &size)
			? size.QuadPart
			: 0;
#else
		return handle.GetSize();
#endif
	}

	[[gnu::pure]]
	uint_least64_t GetPosition() const noexcept {
#ifdef _WIN32
		LARGE_INTEGER zero;
		zero.QuadPart = 0;
		LARGE_INTEGER position;
		return SetFilePointerEx(handle.Get(), zero, &position, FILE_CURRENT)
			? position.QuadPart
			: 0;
#else
		return handle.Tell();
#endif
	}

	void Rewind() {
		Seek(0);
	}

	void Seek(off_t offset);
	void Skip(off_t offset);

	/* virtual methods from class Reader */
	std::size_t Read(std::span<std::byte> dest) override;
};
