/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_DAEMON_HXX
#define MPD_DAEMON_HXX

class AllocatedPath;

#ifndef WIN32
void
daemonize_init(const char *user, const char *group, AllocatedPath &&pidfile);
#else
static inline void
daemonize_init(const char *user, const char *group, AllocatedPath &&pidfile)
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
#include "system/FatalError.hxx"
static inline void
daemonize_kill(void)
{
	FatalError("--kill is not available on WIN32");
}
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
daemonize_begin(bool detach);
#else
static inline void
daemonize_begin(bool detach)
{ (void)detach; }
#endif

#ifndef WIN32
void
daemonize_commit();
#else
static inline void
daemonize_commit() {}
#endif

#endif
