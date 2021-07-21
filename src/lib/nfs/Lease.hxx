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

#ifndef MPD_NFS_LEASE_HXX
#define MPD_NFS_LEASE_HXX

#include <exception>

class NfsLease {
public:
	/**
	 * The #NfsConnection has successfully mounted the server's
	 * export and is ready for regular operation.
	 */
	virtual void OnNfsConnectionReady() noexcept = 0;

	/**
	 * The #NfsConnection has failed to mount the server's export.
	 * This is being called instead of OnNfsConnectionReady().
	 */
	virtual void OnNfsConnectionFailed(std::exception_ptr e) noexcept = 0;

	/**
	 * The #NfsConnection has failed after OnNfsConnectionReady()
	 * had been called already.
	 */
	virtual void OnNfsConnectionDisconnected(std::exception_ptr e) noexcept = 0;
};

#endif
