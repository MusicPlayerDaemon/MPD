/*
 * Copyright 2012-2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef UNIQUE_FILE_DESCRIPTOR_HXX
#define UNIQUE_FILE_DESCRIPTOR_HXX

#include "FileDescriptor.hxx" // IWYU pragma: export

#include <cassert>
#include <utility>

/**
 * An OO wrapper for a UNIX file descriptor.
 */
class UniqueFileDescriptor : public FileDescriptor {
public:
	UniqueFileDescriptor() noexcept
		:FileDescriptor(FileDescriptor::Undefined()) {}

	explicit UniqueFileDescriptor(int _fd) noexcept
		:FileDescriptor(_fd) {}

	explicit UniqueFileDescriptor(FileDescriptor _fd) noexcept
		:FileDescriptor(_fd) {}

	UniqueFileDescriptor(const UniqueFileDescriptor &) = delete;

	UniqueFileDescriptor(UniqueFileDescriptor &&other) noexcept
		:FileDescriptor(other.Steal()) {}

	~UniqueFileDescriptor() noexcept {
		Close();
	}

	UniqueFileDescriptor &operator=(UniqueFileDescriptor &&other) noexcept {
		using std::swap;
		swap(fd, other.fd);
		return *this;
	}

	/**
	 * Release ownership and return the descriptor as an unmanaged
	 * #FileDescriptor instance.
	 */
	FileDescriptor Release() noexcept {
		return std::exchange(*(FileDescriptor *)this, Undefined());
	}

protected:
	void Set(int _fd) noexcept {
		assert(!IsDefined());
		assert(_fd >= 0);

		FileDescriptor::Set(_fd);
	}

public:
#ifndef _WIN32
	static bool CreatePipe(UniqueFileDescriptor &r, UniqueFileDescriptor &w) noexcept {
		return FileDescriptor::CreatePipe(r, w);
	}

	static bool CreatePipeNonBlock(UniqueFileDescriptor &r,
				       UniqueFileDescriptor &w) noexcept {
		return FileDescriptor::CreatePipeNonBlock(r, w);
	}

	static bool CreatePipe(FileDescriptor &r, FileDescriptor &w) noexcept;
#endif

	bool Close() noexcept {
		return IsDefined() && FileDescriptor::Close();
	}
};

#endif
