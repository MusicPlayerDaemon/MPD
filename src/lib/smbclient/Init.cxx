/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "Init.hxx"
#include "Mutex.hxx"
#include "thread/Mutex.hxx"
#include "system/Error.hxx"
#include "config/Block.hxx"

#include <libsmbclient.h>

#include <string.h>

static int log_level= 0;

static void
mpd_smbc_get_auth_data(gcc_unused const char *srv,
		       gcc_unused const char *shr,
		       char *wg, gcc_unused int wglen,
		       char *un, gcc_unused int unlen,
		       char *pw, gcc_unused int pwlen)
{
	// TODO: implement
	strcpy(wg, "WORKGROUP");
	strcpy(un, "");
	strcpy(pw, "");
}

void SmbclientConfig(const ConfigBlock &block)
{
	log_level = block.GetBlockValue("log_level", log_level);
}

void
SmbclientInit()
{
	const std::lock_guard<Mutex> protect(smbclient_mutex);

	if (smbc_init(mpd_smbc_get_auth_data, log_level) < 0)
		throw MakeErrno("smbc_init() failed");
	SMBCCTX *context = smbc_set_context(NULL);
	smbc_setOptionBrowseMaxLmbCount (context, 3);
	smbc_setTimeout(context, 5000);
	smbc_setOptionUseCCache(context, false);
}

void
SmbclientReinit()
{
	const std::lock_guard<Mutex> protect(smbclient_mutex);

	SMBCCTX *context = smbc_new_context();
	if (!context) {
		throw MakeErrno("smbc_new_context() failed");
	}

	smbc_setDebug(context, log_level);
	smbc_setFunctionAuthData(context, mpd_smbc_get_auth_data);
	smbc_setOptionBrowseMaxLmbCount (context, 3);
	smbc_setTimeout(context, 5000);
	smbc_setOptionUseCCache(context, false);

	if (!smbc_init_context(context)) {
		smbc_free_context(context, false);
		throw MakeErrno("smbc_init_context() failed");
	}
	SMBCCTX *old_context = smbc_set_context(NULL);
	if (old_context) {
		if (smbc_free_context(old_context, 1)) {
			throw MakeErrno("smbc_free_context() failed");
		}
	}
	smbc_set_context(context);;
}
