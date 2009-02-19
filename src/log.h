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

#ifndef MPD_LOG_H
#define MPD_LOG_H

#include <glib.h>
#include <stdbool.h>

G_GNUC_PRINTF(1, 2) void DEBUG(const char *fmt, ...);
G_GNUC_PRINTF(1, 2) G_GNUC_NORETURN void FATAL(const char *fmt, ...);

/**
 * Configure a logging destination for daemon startup, before the
 * configuration file is read.  This allows the daemon to use the
 * logging library (and the command line verbose level) before it's
 * daemonized.
 *
 * @param verbose true when the program is started with --verbose
 */
void
log_early_init(bool verbose);

void log_init(bool verbose, bool use_stdout);

void setup_log_output(bool use_stdout);

int cycle_log_files(void);

void close_log_files(void);

#endif /* LOG_H */
