/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "Domain.hxx"
#include "thread/Mutex.hxx"
#include "util/Error.hxx"

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ixml.h>

static Mutex upnp_init_mutex;
static unsigned upnp_ref;

static bool
DoInit(Error &error)
{
	auto code = UpnpInit(0, 0);
	if (code != UPNP_E_SUCCESS) {
		error.Format(upnp_domain, code,
			     "UpnpInit() failed: %s",
			     UpnpGetErrorMessage(code));
		return false;
	}

	UpnpSetMaxContentLength(2000*1024);

	// Servers sometimes make error (e.g.: minidlna returns bad utf-8)
	ixmlRelaxParser(1);

	return true;
}

bool
UpnpGlobalInit(Error &error)
{
	const ScopeLock protect(upnp_init_mutex);

	if (upnp_ref == 0 && !DoInit(error))
		return false;

	++upnp_ref;
	return true;
}

void
UpnpGlobalFinish()
{
	const ScopeLock protect(upnp_init_mutex);

	assert(upnp_ref > 0);

	if (--upnp_ref == 0)
		UpnpFinish();
}
