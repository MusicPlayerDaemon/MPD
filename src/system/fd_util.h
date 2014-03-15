/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

/*
 * This library provides easy helper functions for working with file
 * descriptors.  It has wrappers for taking advantage of Linux 2.6
 * specific features like O_CLOEXEC.
 *
 */

#ifndef FD_UTIL_H
#define FD_UTIL_H

#include "check.h"

#include <stdbool.h>
#include <stddef.h>

#ifndef WIN32
#include <sys/types.h>
#endif

struct sockaddr;

#ifdef __cplusplus
extern "C" {
#endif

int
fd_set_cloexec(int fd, bool enable);

/**
 * Wrapper for dup(), which sets the CLOEXEC flag on the new
 * descriptor.
 */
int
dup_cloexec(int oldfd);

/**
 * Wrapper for open(), which sets the CLOEXEC flag (atomically if
 * supported by the OS).
 */
int
open_cloexec(const char *path_fs, int flags, int mode);

/**
 * Wrapper for pipe(), which sets the CLOEXEC flag (atomically if
 * supported by the OS).
 */
int
pipe_cloexec(int fd[2]);

/**
 * Wrapper for pipe(), which sets the CLOEXEC flag (atomically if
 * supported by the OS).
 *
 * On systems that supports it (everybody except for Windows), it also
 * sets the NONBLOCK flag.
 */
int
pipe_cloexec_nonblock(int fd[2]);

#ifndef WIN32

/**
 * Wrapper for socketpair(), which sets the CLOEXEC flag (atomically
 * if supported by the OS).
 */
int
socketpair_cloexec(int domain, int type, int protocol, int sv[2]);

/**
 * Wrapper for socketpair(), which sets the flags CLOEXEC and NONBLOCK
 * (atomically if supported by the OS).
 */
int
socketpair_cloexec_nonblock(int domain, int type, int protocol, int sv[2]);

#endif

#ifdef HAVE_LIBMPDCLIENT
/* Avoid symbol conflict with statically linked libmpdclient */
#define socket_cloexec_nonblock socket_cloexec_nonblock_noconflict
#endif

/**
 * Wrapper for socket(), which sets the CLOEXEC and the NONBLOCK flag
 * (atomically if supported by the OS).
 */
int
socket_cloexec_nonblock(int domain, int type, int protocol);

/**
 * Wrapper for accept(), which sets the CLOEXEC and the NONBLOCK flags
 * (atomically if supported by the OS).
 */
int
accept_cloexec_nonblock(int fd, struct sockaddr *address,
			size_t *address_length_r);


#ifndef WIN32

struct msghdr;

/**
 * Wrapper for recvmsg(), which sets the CLOEXEC flag (atomically if
 * supported by the OS).
 */
ssize_t
recvmsg_cloexec(int sockfd, struct msghdr *msg, int flags);

#endif

#ifdef HAVE_INOTIFY_INIT

/**
 * Wrapper for inotify_init(), which sets the CLOEXEC flag (atomically
 * if supported by the OS).
 */
int
inotify_init_cloexec(void);

#endif

#ifdef USE_EVENTFD

/**
 * Wrapper for eventfd() which sets the flags CLOEXEC and NONBLOCK
 * flag (atomically if supported by the OS).
 */
int
eventfd_cloexec_nonblock(unsigned initval, int flags);

#endif

/**
 * Portable wrapper for close(); use closesocket() on WIN32/WinSock.
 */
int
close_socket(int fd);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
