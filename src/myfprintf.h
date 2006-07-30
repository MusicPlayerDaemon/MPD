/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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

#ifndef MYFPRINTF_H
#define MYFPRINTF_H

#include "../config.h"
#include "gcc.h"

#include <stdarg.h>
#include <stdio.h>

void myfprintfStdLogMode(FILE * out, FILE * err);

mpd_fprintf void fdprintf(const int fd, const char *fmt, ...);
void vfdprintf(const int fd, const char *fmt, va_list arglist);

#define myfprintf(fp, ...) do { \
		fprintf(fp, __VA_ARGS__); \
		fflush(fp); \
	} while (0)

int myfprintfCloseAndOpenLogFile();

void myfprintfCloseLogFile();
#endif
