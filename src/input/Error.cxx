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

#include "Error.hxx"
#include "system/Error.hxx"
#include "config.h"

#ifdef ENABLE_CURL
#include "lib/curl/Error.hxx"
#endif

#ifdef ENABLE_NFS
#include "lib/nfs/Error.hxx"
#include <nfsc/libnfs-raw-nfs.h>
#endif

#include <utility>

bool
IsFileNotFound(std::exception_ptr ep) noexcept
{
	try {
		std::rethrow_exception(std::move(ep));
	} catch (const std::system_error &e) {
		return IsFileNotFound(e);
#ifdef ENABLE_CURL
	} catch (const HttpStatusError &e) {
		return e.GetStatus() == 404;
#endif
#ifdef ENABLE_NFS
	} catch (const NfsClientError &e) {
		return e.GetCode() == NFS3ERR_NOENT;
#endif
	} catch (...) {
	}

	return false;
}

