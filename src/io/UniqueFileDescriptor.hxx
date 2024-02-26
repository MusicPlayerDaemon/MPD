// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

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
