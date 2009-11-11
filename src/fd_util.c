/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
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
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h" /* must be first for large file support */
#include "fd_util.h"

#if !defined(_GNU_SOURCE) && (defined(HAVE_PIPE2) || defined(HAVE_ACCEPT4))
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#ifdef HAVE_INOTIFY_INIT
#include <sys/inotify.h>
#endif

#ifndef WIN32

static int
fd_mask_flags(int fd, int and_mask, int xor_mask)
{
	int ret;

	assert(fd >= 0);

	ret = fcntl(fd, F_GETFD, 0);
	if (ret < 0)
		return ret;

	return fcntl(fd, F_SETFD, (ret & and_mask) ^ xor_mask);
}

#endif /* !WIN32 */

static int
fd_set_cloexec(int fd, bool enable)
{
#ifndef WIN32
	return fd_mask_flags(fd, ~FD_CLOEXEC, enable ? FD_CLOEXEC : 0);
#else
	(void)fd;
	(void)enable;
#endif
}

/**
 * Enables non-blocking mode for the specified file descriptor.  On
 * WIN32, this function only works for sockets.
 */
static int
fd_set_nonblock(int fd)
{
#ifdef WIN32
	u_long val = 1;
	return ioctlsocket(fd, FIONBIO, &val);
#else
	int flags;

	assert(fd >= 0);

	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		return flags;

	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

int
open_cloexec(const char *path_fs, int flags, int mode)
{
	int fd;

#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif

#ifdef O_NOCTTY
	flags |= O_NOCTTY;
#endif

	fd = open(path_fs, flags, mode);
	if (fd >= 0)
		fd_set_cloexec(fd, true);

	return fd;
}

int
pipe_cloexec(int fd[2])
{
#ifdef WIN32
	return _pipe(event_pipe, 512, _O_BINARY);
#else
	int ret;

#ifdef HAVE_PIPE2
	ret = pipe2(fd, O_CLOEXEC);
	if (ret >= 0 || errno != ENOSYS)
		return ret;
#endif

	ret = pipe(fd);
	if (ret >= 0) {
		fd_set_cloexec(fd[0], true);
		fd_set_cloexec(fd[1], true);
	}

	return ret;
#endif
}

int
pipe_cloexec_nonblock(int fd[2])
{
#ifdef WIN32
	return _pipe(event_pipe, 512, _O_BINARY);
#else
	int ret;

#ifdef HAVE_PIPE2
	ret = pipe2(fd, O_CLOEXEC|O_NONBLOCK);
	if (ret >= 0 || errno != ENOSYS)
		return ret;
#endif

	ret = pipe(fd);
	if (ret >= 0) {
		fd_set_cloexec(fd[0], true);
		fd_set_cloexec(fd[1], true);

		fd_set_nonblock(fd[0]);
		fd_set_nonblock(fd[1]);
	}

	return ret;
#endif
}

int
socket_cloexec_nonblock(int domain, int type, int protocol)
{
	int fd;

#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
	fd = socket(domain, type | SOCK_CLOEXEC | SOCK_NONBLOCK, protocol);
	if (fd >= 0 || errno != EINVAL)
		return fd;
#endif

	fd = socket(domain, type, protocol);
	if (fd >= 0) {
		fd_set_cloexec(fd, true);
		fd_set_nonblock(fd);
	}

	return fd;
}

int
accept_cloexec_nonblock(int fd, struct sockaddr *address,
			size_t *address_length_r)
{
	int ret;
	socklen_t address_length = *address_length_r;

#ifdef HAVE_ACCEPT4
	ret = accept4(fd, address, &address_length,
		      SOCK_CLOEXEC|SOCK_NONBLOCK);
	if (ret >= 0 || errno != ENOSYS) {
		if (ret >= 0)
			*address_length_r = address_length;

		return ret;
	}
#endif

	ret = accept(fd, address, &address_length);
	if (ret >= 0) {
		fd_set_cloexec(ret, true);
		fd_set_nonblock(ret);
		*address_length_r = address_length;
	}

	return ret;
}

#ifdef HAVE_INOTIFY_INIT

int
inotify_init_cloexec(void)
{
	int fd;

#ifdef HAVE_INOTIFY_INIT1
	fd = inotify_init1(IN_CLOEXEC);
	if (fd >= 0 || errno != ENOSYS)
		return fd;
#endif

	fd = inotify_init();
	if (fd >= 0)
		fd_set_cloexec(fd, true);

	return fd;
}

#endif
