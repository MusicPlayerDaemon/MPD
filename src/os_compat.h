/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef OS_COMPAT_H
#define OS_COMPAT_H

/*
 * This includes OS-wide headers that can be expected to be available
 * on any machine that mpd can be compiled on for any UNIX-like OS.
 *
 * This does not include headers for optional dependencies such as
 * those for:
 * 1) input/output plugins
 * 2) optional features in core (libsamplerate, avahi, ...)
 */

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#elif defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#elif defined(HAVE_SYS_INTTYPES_H)
#include <sys/inttypes.h>
#endif
#include <sys/types.h>

#define _XOPEN_SOURCE 600 /* for posix_fadvise, won't hurt if not available */
#include <fcntl.h>

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/param.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <stdarg.h>
#include <dirent.h>
#include <math.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>
#include <stddef.h> /* needed? this defines NULL + offsetof() */
#include <resolv.h>

/* remove when we switch to pthreads: */
#include <sys/ipc.h>
#include <sys/shm.h>

#ifdef HAVE_UN
#include <sys/un.h>
#endif

#endif /* OS_COMPAT_H */
