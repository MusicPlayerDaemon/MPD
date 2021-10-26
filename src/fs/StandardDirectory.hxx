/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "AllocatedPath.hxx"

/**
 * Obtains configuration directory for the current user.
 */
[[gnu::const]]
AllocatedPath
GetUserConfigDir() noexcept;

/**
 * Obtains music directory for the current user.
 */
[[gnu::const]]
AllocatedPath
GetUserMusicDir() noexcept;

/**
 * Obtains cache directory for the current user.
 */
[[gnu::const]]
AllocatedPath
GetUserCacheDir() noexcept;

/**
 * Obtains the runtime directory for the current user.
 */
[[gnu::const]]
AllocatedPath
GetUserRuntimeDir() noexcept;

/**
 * Obtains the runtime directory for this application.
 */
[[gnu::const]]
AllocatedPath
GetAppRuntimeDir() noexcept;

#ifdef _WIN32

/**
 * Obtains system configuration directory.
 */
[[gnu::const]]
AllocatedPath
GetSystemConfigDir() noexcept;

/**
 * Obtains application application base directory.
 * Application base directory is a directory that contains 'bin' folder
 * for current executable.
 */
[[gnu::const]]
AllocatedPath
GetAppBaseDir() noexcept;

#else

/**
 * Obtains home directory for the current user.
 */
[[gnu::const]]
AllocatedPath
GetHomeDir() noexcept;

/**
 * Obtains home directory for the specified user.
 */
[[gnu::pure]]
AllocatedPath
GetHomeDir(const char *user_name) noexcept;

#endif

#endif
