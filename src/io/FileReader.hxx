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
