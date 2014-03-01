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
 
#ifndef MPD_FS_STANDARD_DIRECTORY_HXX
#define MPD_FS_STANDARD_DIRECTORY_HXX

#include "check.h"
#include "AllocatedPath.hxx"

/**
 * Obtains configuration directory for the current user.
 */
AllocatedPath GetUserConfigDir();

/**
 * Obtains music directory for the current user.
 */
AllocatedPath GetUserMusicDir();

/**
 * Obtains cache directory for the current user.
 */
gcc_pure
AllocatedPath
GetUserCacheDir();

#ifdef WIN32

/**
 * Obtains system configuration directory.
 */
AllocatedPath GetSystemConfigDir();

/**
 * Obtains application application base directory.
 * Application base directory is a directory that contains 'bin' folder
 * for current executable.
 */
AllocatedPath GetAppBaseDir();

#else

/**
 * Obtains home directory for the current user.
 */
AllocatedPath GetHomeDir();

/**
 * Obtains home directory for the specified user.
 */
AllocatedPath GetHomeDir(const char *user_name);

#endif

#endif
