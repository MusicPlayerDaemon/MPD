// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

class AllocatedPath;

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
 * Obtains cache directory for this application.
 */
[[gnu::const]]
AllocatedPath
GetAppCacheDir() noexcept;

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
