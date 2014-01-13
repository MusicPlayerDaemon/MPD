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

#ifndef MPD_CHECK_H
#define MPD_CHECK_H

/*
 * All sources must include config.h on the first line to ensure that
 * Large File Support is configured properly.  This header checks
 * whether this has happened.
 *
 * Usage: include this header before you use any of the above types.
 * It will stop the compiler if something went wrong.
 *
 * This is Linux/glibc specific, and only enabled in the debug build,
 * so bugs in this headers don't affect users with production builds.
 *
 */

#ifndef PACKAGE_VERSION
#error config.h missing
#endif

#if defined(__linux__) && !defined(NDEBUG) && defined(ENABLE_LARGEFILE) && \
	defined(_FEATURES_H) && defined(__i386__) && \
	!defined(__USE_FILE_OFFSET64)
/* on i386, check if LFS is enabled */
#error config.h was included too late
#endif

#endif
