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

#include "config.h"
#include "FileDescriptor.hxx"

#include <sys/stat.h>
#include <fcntl.h>

#ifndef _WIN32
#include <poll.h>
#endif

#ifdef USE_EVENTFD
#include <sys/eventfd.h>
#endif

#ifdef USE_SIGNALFD
#include <sys/signalfd.h>
#endif

#ifdef HAVE_INOTIFY_INIT
#include <sys/inotify.h>
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#ifndef _WIN32

bool
FileDescriptor::IsValid() const noexcept
{
	return IsDefined() && fcntl(fd, F_GETFL) >= 0;
}

bool
FileDescriptor::IsPipe() const noexcept
{
	struct stat st;
	return IsDefined() && fstat(fd, &st) == 0 && S_ISFIFO(st.st_mode);
}

bool
FileDescriptor::IsSocket() const noexcept
{
	struct stat st;
	return IsDefined() && fstat(fd, &st) == 0 && S_ISSOCK(st.st_mode);
}

#endif

bool
FileDescriptor::Open(const char *pathname, int flags, mode_t mode) noexcept
{
	fd = ::open(pathname, flags | O_NOCTTY | O_CLOEXEC, mode);
	return IsDefined();
}

#ifdef _WIN32

bool
FileDescriptor::Open(const wchar_t *pathname, int flags, mode_t mode) noexcept
{
	fd = ::_wopen(pathname, flags | O_NOCTTY | O_CLOEXEC, mode);
	return IsDefined();
}

#endif

bool
FileDescriptor::OpenReadOnly(const char *pathname) noexcept
{
	return Open(pathname, O_RDONLY);
}

#ifndef _WIN32

bool
FileDescriptor::OpenNonBlocking(const char *pathname) noexcept
{
	return Open(pathname, O_RDWR | O_NONBLOCK);
}

#endif

bool
FileDescriptor::CreatePipe(FileDescriptor &r, FileDescriptor &w) noexcept
{
	int fds[2];

#ifdef HAVE_PIPE2
	const int flags = O_CLOEXEC;
	const int result = pipe2(fds, flags);
#elif defined(_WIN32)
	const int result = _pipe(fds, 512, _O_BINARY);
#else
	const int result = pipe(fds);
#endif

	if (result < 0)
		return false;

	r = FileDescriptor(fds[0]);
	w = FileDescriptor(fds[1]);
	return true;
}

#ifndef _WIN32

bool
FileDescriptor::CreatePipeNonBlock(FileDescriptor &r,
				   FileDescriptor &w) noexcept
{
	int fds[2];

#ifdef HAVE_PIPE2
	const int flags = O_CLOEXEC|O_NONBLOCK;
	const int result = pipe2(fds, flags);
#else
	const int result = pipe(fds);
#endif

	if (result < 0)
		return false;

	r = FileDescriptor(fds[0]);
	w = FileDescriptor(fds[1]);

#ifndef HAVE_PIPE2
	r.SetNonBlocking();
	w.SetNonBlocking();
#endif

	return true;
}

void
FileDescriptor::SetNonBlocking() noexcept
{
	assert(IsDefined());

	int flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void
FileDescriptor::SetBlocking() noexcept
{
	assert(IsDefined());

	int flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

void
FileDescriptor::EnableCloseOnExec() noexcept
{
	assert(IsDefined());

	const int old_flags = fcntl(fd, F_GETFD, 0);
	fcntl(fd, F_SETFD, old_flags | FD_CLOEXEC);
}

void
FileDescriptor::DisableCloseOnExec() noexcept
{
	assert(IsDefined());

	const int old_flags = fcntl(fd, F_GETFD, 0);
	fcntl(fd, F_SETFD, old_flags & ~FD_CLOEXEC);
}

bool
FileDescriptor::CheckDuplicate(int new_fd) noexcept
{
	if (fd == new_fd) {
		DisableCloseOnExec();
		return true;
	} else
		return Duplicate(new_fd);
}

#endif

#ifdef USE_EVENTFD

bool
FileDescriptor::CreateEventFD(unsigned initval) noexcept
{
	fd = ::eventfd(initval, EFD_NONBLOCK|EFD_CLOEXEC);
	return fd >= 0;
}

#endif

#ifdef USE_SIGNALFD

bool
FileDescriptor::CreateSignalFD(const sigset_t *mask) noexcept
{
	int new_fd = ::signalfd(fd, mask, SFD_NONBLOCK|SFD_CLOEXEC);
	if (new_fd < 0)
		return false;

	fd = new_fd;
	return true;
}

#endif

#ifdef HAVE_INOTIFY_INIT

bool
FileDescriptor::CreateInotify() noexcept
{
#ifdef HAVE_INOTIFY_INIT1
	int new_fd = inotify_init1(IN_CLOEXEC|IN_NONBLOCK);
#else
	int new_fd = inotify_init();
#endif
	if (new_fd < 0)
		return false;

#ifndef HAVE_INOTIFY_INIT1
	SetNonBlocking();
#endif

	fd = new_fd;
	return true;
}

#endif

bool
FileDescriptor::Rewind() noexcept
{
	assert(IsDefined());

	return lseek(fd, 0, SEEK_SET) == 0;
}

off_t
FileDescriptor::GetSize() const noexcept
{
	struct stat st;
	return ::fstat(fd, &st) >= 0
		? (long)st.st_size
		: -1;
}

#ifndef _WIN32

int
FileDescriptor::Poll(short events, int timeout) const noexcept
{
	assert(IsDefined());

	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = events;
	int result = poll(&pfd, 1, timeout);
	return result > 0
		? pfd.revents
		: result;
}

int
FileDescriptor::WaitReadable(int timeout) const noexcept
{
	return Poll(POLLIN, timeout);
}

int
FileDescriptor::WaitWritable(int timeout) const noexcept
{
	return Poll(POLLOUT, timeout);
}

bool
FileDescriptor::IsReadyForWriting() const noexcept
{
	return WaitWritable(0) > 0;
}

#endif
