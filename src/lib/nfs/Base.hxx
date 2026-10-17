// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include <string_view>

/**
 * Register a "base" NFS server and export name.  This will be the
 * export that will be mounted if a file within this export is being
 * opened, instead of guessing the mount point.
 *
 * @param owner an opaque pointer identifying the caller, to be
 * passed to nfs_remove_base()
 */
void
nfs_add_base(const void *owner,
	     std::string_view server, std::string_view export_name) noexcept;

/**
 * Unregister the "base" which was registered by nfs_add_base() with
 * the given owner.
 */
void
nfs_remove_base(const void *owner) noexcept;

/**
 * Check if the given server and path are inside one of the "base"
 * server/export_name pairs.  If yes, then a pointer to the portion of
 * "path" after the export_name is returned; otherwise, nullptr is
 * returned.
 */
const char *
nfs_check_base(std::string_view server, const char *path) noexcept;
