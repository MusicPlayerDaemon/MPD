/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "Init.hxx"
#include "Mutex.hxx"
#include "thread/Mutex.hxx"
#include "system/Error.hxx"

#include <libsmbclient.h>

#include <cstring>

static void
mpd_smbc_get_auth_data([[maybe_unused]] const char *srv,
		       [[maybe_unused]] const char *shr,
		       char *wg, [[maybe_unused]] int wglen,
		       char *un, [[maybe_unused]] int unlen,
		       char *pw, [[maybe_unused]] int pwlen)
{
	// TODO: implement
	std::strcpy(wg, "WORKGROUP");
	std::strcpy(un, "");
	std::strcpy(pw, "");
}

void
SmbclientInit()
{
	const std::lock_guard<Mutex> protect(smbclient_mutex);

	constexpr int debug = 0;
	if (smbc_init(mpd_smbc_get_auth_data, debug) < 0)
		throw MakeErrno("smbc_init() failed");
}
