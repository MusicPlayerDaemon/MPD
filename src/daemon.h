/*
 * Copyright (C) 2003-2008 The Music Player Daemon Project
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef DAEMON_H
#define DAEMON_H

#include <stdbool.h>

void
daemonize_init(const char *user, const char *pidfile);

void
daemonize_finish(void);

/**
 * Kill the MPD which is currently running, pid determined from the
 * pid file.
 */
void
daemonize_kill(void);

/**
 * Close stdin (fd 0) and re-open it as /dev/null.
 */
void
daemonize_close_stdin(void);

/**
 * Change to the configured Unix user.
 */
void
daemonize_set_user(void);

void
daemonize(bool detach);

#endif
