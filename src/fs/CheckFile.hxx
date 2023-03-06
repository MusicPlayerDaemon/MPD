// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FS_CHECK_FILE_HXX
#define MPD_FS_CHECK_FILE_HXX

class Path;

/**
 * Check whether the directory is readable and usable.  Logs a warning
 * if there is a problem.
 */
void
CheckDirectoryReadable(Path path_fs);

#endif
