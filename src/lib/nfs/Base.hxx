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
