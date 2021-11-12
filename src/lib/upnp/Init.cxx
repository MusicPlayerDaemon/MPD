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

#include "Init.hxx"
#include "Compat.hxx"
#include "thread/Mutex.hxx"
#include "util/RuntimeError.hxx"

#include <upnptools.h>
#ifdef USING_PUPNP
#	include <ixml.h>
#endif

#include <cassert>

static Mutex upnp_init_mutex;
static unsigned upnp_ref;

static void
DoInit(const char* iface)
{

#ifdef UPNP_ENABLE_IPV6
	auto code = UpnpInit2(iface, 0);
#else
	auto code = UpnpInit(iface, 0);
#endif
	if (code != UPNP_E_SUCCESS)
		throw FormatRuntimeError("UpnpInit() failed: %s",
					 UpnpGetErrorMessage(code));

	UpnpSetMaxContentLength(2000*1024);

#ifdef USING_PUPNP
	// Servers sometimes make error (e.g.: minidlna returns bad utf-8)
	ixmlRelaxParser(1);
#endif
}

void
UpnpGlobalInit(const char* iface)
{
	const std::scoped_lock<Mutex> protect(upnp_init_mutex);

	if (upnp_ref == 0)
		DoInit(iface);

	++upnp_ref;
}

void
UpnpGlobalFinish() noexcept
{
	const std::scoped_lock<Mutex> protect(upnp_init_mutex);

	assert(upnp_ref > 0);

	if (--upnp_ref == 0)
		UpnpFinish();
}
