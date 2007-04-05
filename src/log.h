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

#ifndef LOG_H
#define LOG_H

#include "../config.h"
#include "gcc.h"

#include <unistd.h>

#define LOG_LEVEL_LOW		0
#define LOG_LEVEL_SECURE	1
#define LOG_LEVEL_DEBUG		2

mpd_printf void ERROR(const char *fmt, ...);
mpd_printf void LOG(const char *fmt, ...);
mpd_printf void SECURE(const char *fmt, ...);
mpd_printf void DEBUG(const char *fmt, ...);
mpd_printf void WARNING(const char *fmt, ...);
mpd_printf void FATAL(const char *fmt, ...);

void initLog(const int verbose);

void setup_log_output(const int use_stdout);

void open_log_files(const int use_stdout);

int cycle_log_files(void);

void close_log_files(void);

void flushWarningLog(void);

#endif /* LOG_H */
