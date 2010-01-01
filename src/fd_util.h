/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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

#include <stdbool.h>
#include <stddef.h>

struct sockaddr;

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

/**
 * Wrapper for inotify_init(), which sets the CLOEXEC flag (atomically
 * if supported by the OS).
 */
int
inotify_init_cloexec(void);

#endif
