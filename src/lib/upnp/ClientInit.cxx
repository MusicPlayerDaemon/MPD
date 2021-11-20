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

#include "ClientInit.hxx"
#include "Init.hxx"
#include "Callback.hxx"
#include "thread/Mutex.hxx"
#include "util/RuntimeError.hxx"

#include <upnptools.h>

#include <cassert>

static Mutex upnp_client_init_mutex;
static unsigned upnp_client_ref;
static UpnpClient_Handle upnp_client_handle;

static int
UpnpClientCallback(Upnp_EventType et,
		   const void *evp,
		   void *cookie) noexcept
{
	if (cookie == nullptr)
		/* this is the cookie passed to UpnpRegisterClient();
		   but can this ever happen?  Will libupnp ever invoke
		   the registered callback without that cookie? */
		return UPNP_E_SUCCESS;

	UpnpCallback &callback = UpnpCallback::FromUpnpCookie(cookie);
	return callback.Invoke(et, evp);
}

static void
DoInit()
{
	auto code = UpnpRegisterClient(UpnpClientCallback, nullptr,
				       &upnp_client_handle);
	if (code != UPNP_E_SUCCESS)
		throw FormatRuntimeError("UpnpRegisterClient() failed: %s",
					 UpnpGetErrorMessage(code));
}

UpnpClient_Handle
UpnpClientGlobalInit(const char* iface)
{
	UpnpGlobalInit(iface);

	try {
		const std::scoped_lock<Mutex> protect(upnp_client_init_mutex);
		if (upnp_client_ref == 0)
			DoInit();
	} catch (...) {
		UpnpGlobalFinish();
		throw;
	}

	++upnp_client_ref;
	return upnp_client_handle;
}

void
UpnpClientGlobalFinish() noexcept
{
	{
		const std::scoped_lock<Mutex> protect(upnp_client_init_mutex);

		assert(upnp_client_ref > 0);
		if (--upnp_client_ref == 0)
			UpnpUnRegisterClient(upnp_client_handle);
	}

	UpnpGlobalFinish();
}
