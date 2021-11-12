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

#include "Context.hxx"
#include "system/Error.hxx"

#include <cerrno>

#include <string.h>

Mutex SmbclientContext::global_mutex;

static void
mpd_smbc_get_auth_data([[maybe_unused]] const char *srv,
		       [[maybe_unused]] const char *shr,
		       char *wg, [[maybe_unused]] int wglen,
		       char *un, [[maybe_unused]] int unlen,
		       char *pw, [[maybe_unused]] int pwlen)
{
	// TODO: implement
	strcpy(wg, "WORKGROUP");
	strcpy(un, "");
	strcpy(pw, "");
}

SmbclientContext
SmbclientContext::New()
{
	SMBCCTX *ctx;

	{
		const std::scoped_lock<Mutex> protect(global_mutex);
		ctx = smbc_new_context();
	}

	if (ctx == nullptr)
		throw MakeErrno("smbc_new_context() failed");

	constexpr int debug = 0;
	smbc_setDebug(ctx, debug);
	smbc_setFunctionAuthData(ctx, mpd_smbc_get_auth_data);

	SMBCCTX *ctx2 = smbc_init_context(ctx);
	if (ctx2 == nullptr) {
		int e = errno;
		smbc_free_context(ctx, 1);
		throw MakeErrno(e, "smbc_init_context() failed");
	}

	return SmbclientContext(ctx2);
}
