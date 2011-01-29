/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef DAEMON_H
#define DAEMON_H

#include "mpd_error.h"

#include <stdbool.h>

#ifndef WIN32
void
daemonize_init(const char *user, const char *group, const char *pidfile);
#else
static inline void
daemonize_init(const char *user, const char *group, const char *pidfile)
{ (void)user; (void)group; (void)pidfile; }
#endif

#ifndef WIN32
void
daemonize_finish(void);
#else
static inline void
daemonize_finish(void)
{ /* nop */ }
#endif

/**
 * Kill the MPD which is currently running, pid determined from the
 * pid file.
 */
#ifndef WIN32
void
daemonize_kill(void);
#else
#include <glib.h>
static inline void
daemonize_kill(void)
{ MPD_ERROR("--kill is not available on WIN32"); }
#endif

/**
 * Close stdin (fd 0) and re-open it as /dev/null.
 */
#ifndef WIN32
void
daemonize_close_stdin(void);
#else
static inline void
daemonize_close_stdin(void) {}
#endif

/**
 * Change to the configured Unix user.
 */
#ifndef WIN32
void
daemonize_set_user(void);
#else
static inline void
daemonize_set_user(void)
{ /* nop */ }
#endif

#ifndef WIN32
void
daemonize(bool detach);
#else
static inline void
daemonize(bool detach)
{ (void)detach; }
#endif

#endif
