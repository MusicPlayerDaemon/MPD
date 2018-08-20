/*
 * Copyright 2012-2018 Max Kellermann <max.kellermann@gmail.com>
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

#include "FileDescriptor.hxx"

#include <utility>

/**
 * An OO wrapper for a UNIX file descriptor.
 */
class UniqueFileDescriptor : protected FileDescriptor {
public:
	UniqueFileDescriptor() noexcept
		:FileDescriptor(FileDescriptor::Undefined()) {}

protected:
	explicit UniqueFileDescriptor(int _fd) noexcept:FileDescriptor(_fd) {
		assert(IsDefined());
	}

public:
	explicit UniqueFileDescriptor(FileDescriptor _fd) noexcept
		:FileDescriptor(_fd) {}

	UniqueFileDescriptor(UniqueFileDescriptor &&other) noexcept
		:FileDescriptor(other.Steal()) {}

	~UniqueFileDescriptor() noexcept {
		Close();
	}

	UniqueFileDescriptor &operator=(UniqueFileDescriptor &&other) noexcept {
		std::swap(fd, other.fd);
		return *this;
	}

	/**
	 * Convert this object to its #FileDescriptor base type.
	 */
	const FileDescriptor &ToFileDescriptor() const noexcept {
		return *this;
	}

	using FileDescriptor::IsDefined;
#ifndef _WIN32
	using FileDescriptor::IsValid;
#endif
	using FileDescriptor::Get;
	using FileDescriptor::Steal;

protected:
	void Set(int _fd) noexcept {
		assert(!IsDefined());
		assert(_fd >= 0);

		FileDescriptor::Set(_fd);
	}

public:
	using FileDescriptor::Open;
	using FileDescriptor::OpenReadOnly;

#ifndef _WIN32
	using FileDescriptor::OpenNonBlocking;

	static bool CreatePipe(UniqueFileDescriptor &r, UniqueFileDescriptor &w) noexcept {
		return FileDescriptor::CreatePipe(r, w);
	}

	using FileDescriptor::SetNonBlocking;
	using FileDescriptor::SetBlocking;
	using FileDescriptor::Duplicate;
	using FileDescriptor::CheckDuplicate;

	static bool CreatePipe(FileDescriptor &r, FileDescriptor &w) noexcept;
#endif

	using FileDescriptor::EnableCloseOnExec;
	using FileDescriptor::DisableCloseOnExec;

#ifdef USE_EVENTFD
	using FileDescriptor::CreateEventFD;
#endif

#ifdef USE_SIGNALFD
	using FileDescriptor::CreateSignalFD;
#endif

#ifdef HAVE_INOTIFY_INIT
	using FileDescriptor::CreateInotify;
#endif

	bool Close() noexcept {
		return IsDefined() && FileDescriptor::Close();
	}

	using FileDescriptor::Rewind;
	using FileDescriptor::Seek;
	using FileDescriptor::Skip;
	using FileDescriptor::Tell;
	using FileDescriptor::GetSize;
	using FileDescriptor::Read;
	using FileDescriptor::Write;

#ifndef _WIN32
	using FileDescriptor::Poll;
	using FileDescriptor::WaitReadable;
	using FileDescriptor::WaitWritable;
	using FileDescriptor::IsReadyForWriting;
#endif
};

#endif
