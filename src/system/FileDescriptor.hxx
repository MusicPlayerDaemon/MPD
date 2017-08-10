/*
 * Copyright (C) 2012-2017 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef FILE_DESCRIPTOR_HXX
#define FILE_DESCRIPTOR_HXX

#include "check.h"
#include "Compiler.h"

#include <utility>

#include <assert.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef USE_SIGNALFD
#include <signal.h>
#endif

/**
 * An OO wrapper for a UNIX file descriptor.
 *
 * This class is unmanaged and trivial.
 */
class FileDescriptor {
protected:
	int fd;

public:
	FileDescriptor() = default;
	explicit constexpr FileDescriptor(int _fd):fd(_fd) {}

	constexpr bool operator==(FileDescriptor other) const {
		return fd == other.fd;
	}

	constexpr bool IsDefined() const {
		return fd >= 0;
	}

#ifndef _WIN32
	/**
	 * Ask the kernel whether this is a valid file descriptor.
	 */
	gcc_pure
	bool IsValid() const noexcept;
#endif

	/**
	 * Returns the file descriptor.  This may only be called if
	 * IsDefined() returns true.
	 */
	constexpr int Get() const {
		return fd;
	}

	void Set(int _fd) noexcept {
		fd = _fd;
	}

	int Steal() noexcept {
		return std::exchange(fd, -1);
	}

	void SetUndefined() noexcept {
		fd = -1;
	}

	static constexpr FileDescriptor Undefined() {
		return FileDescriptor(-1);
	}

	bool Open(const char *pathname, int flags, mode_t mode=0666) noexcept;
	bool OpenReadOnly(const char *pathname) noexcept;

#ifndef WIN32
	bool OpenNonBlocking(const char *pathname) noexcept;

	static bool CreatePipe(FileDescriptor &r, FileDescriptor &w) noexcept;

	/**
	 * Enable non-blocking mode on this file descriptor.
	 */
	void SetNonBlocking() noexcept;

	/**
	 * Enable blocking mode on this file descriptor.
	 */
	void SetBlocking() noexcept;

	/**
	 * Auto-close this file descriptor when a new program is
	 * executed.
	 */
	void EnableCloseOnExec() noexcept;

	/**
	 * Do not auto-close this file descriptor when a new program
	 * is executed.
	 */
	void DisableCloseOnExec() noexcept;

	/**
	 * Duplicate the file descriptor onto the given file descriptor.
	 */
	bool Duplicate(int new_fd) const noexcept {
		return ::dup2(Get(), new_fd) == 0;
	}
#endif

#ifdef USE_EVENTFD
	bool CreateEventFD(unsigned initval=0) noexcept;
#endif

#ifdef USE_SIGNALFD
	bool CreateSignalFD(const sigset_t *mask) noexcept;
#endif

#ifdef HAVE_INOTIFY_INIT
	bool CreateInotify() noexcept;
#endif

	/**
	 * Close the file descriptor.  It is legal to call it on an
	 * "undefined" object.  After this call, IsDefined() is guaranteed
	 * to return false, and this object may be reused.
	 */
	bool Close() noexcept {
		return ::close(Steal()) == 0;
	}

	/**
	 * Rewind the pointer to the beginning of the file.
	 */
	bool Rewind() noexcept;

	off_t Seek(off_t offset) noexcept {
		return lseek(Get(), offset, SEEK_SET);
	}

	off_t Skip(off_t offset) noexcept {
		return lseek(Get(), offset, SEEK_CUR);
	}

	gcc_pure
	off_t Tell() const noexcept {
		return lseek(Get(), 0, SEEK_CUR);
	}

	/**
	 * Returns the size of the file in bytes, or -1 on error.
	 */
	gcc_pure
	off_t GetSize() const noexcept;

	ssize_t Read(void *buffer, size_t length) noexcept {
		return ::read(fd, buffer, length);
	}

	ssize_t Write(const void *buffer, size_t length) noexcept {
		return ::write(fd, buffer, length);
	}

#ifndef WIN32
	int Poll(short events, int timeout) const noexcept;

	int WaitReadable(int timeout) const noexcept;
	int WaitWritable(int timeout) const noexcept;

	gcc_pure
	bool IsReadyForWriting() const noexcept;
#endif
};

#endif
