// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NFS_BASE_HXX
#define MPD_NFS_BASE_HXX

/**
 * Set the "base" NFS server and export name.  This will be the
 * default export that will be mounted if a file within this export is
 * being opened, instead of guessing the mount point.
 *
 * This is a kludge that is not truly thread-safe.
 */
void
nfs_set_base(const char *server, const char *export_name) noexcept;

/**
 * Check if the given server and path are inside the "base"
 * server/export_name.  If yes, then a pointer to the portion of
 * "path" after the export_name is returned; otherwise, nullptr is
 * returned.
 */
[[gnu::pure]]
const char *
nfs_check_base(const char *server, const char *path) noexcept;

#endif
