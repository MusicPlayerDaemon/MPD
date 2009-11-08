/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
 * Wrapper for open(), which sets to CLOEXEC flag (atomically if
 * supported by the OS).
 */
int
open_cloexec(const char *path_fs, int flags);

/**
 * Wrapper for creat(), which sets to CLOEXEC flag (atomically if
 * supported by the OS).
 */
int
creat_cloexec(const char *path_fs, int mode);

/**
 * Wrapper for pipe(), which sets to CLOEXEC flag (atomically if
 * supported by the OS).
 */
int
pipe_cloexec(int fd[2]);

/**
 * Wrapper for socket(), which sets to CLOEXEC flag (atomically if
 * supported by the OS).
 */
int
socket_cloexec(int domain, int type, int protocol);

/**
 * Wrapper for accept(), which sets to CLOEXEC flag (atomically if
 * supported by the OS).
 */
int
accept_cloexec(int fd, struct sockaddr *address, size_t *address_length_r);

/**
 * Wrapper for inotify_init(), which sets to CLOEXEC flag (atomically
 * if supported by the OS).
 */
int
inotify_init_cloexec(void);

#endif
