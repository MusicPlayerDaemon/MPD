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

#ifndef MPD_ERROR_H
#define MPD_ERROR_H

#include <stdlib.h>

/* This macro is used as an intermediate step to a proper error handling
 * using GError in mpd. It is used for unrecoverable error conditions
 * and exits immediately. The long-term goal is to replace this macro by
 * proper error handling. */

#define MPD_ERROR(...) \
	do { \
		g_critical(__VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while(0)

#endif
