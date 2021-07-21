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

#ifndef MPD_UPNP_CALLBACK_HXX
#define MPD_UPNP_CALLBACK_HXX

#include "Compat.hxx"

/**
 * A class that is supposed to be used for libupnp asynchronous
 * callbacks.
 */
class UpnpCallback {
public:
	/**
	 * Pass this value as "cookie" pointer to libupnp asynchronous
	 * functions.
	 */
	void *GetUpnpCookie() noexcept {
		return this;
	}

	static UpnpCallback &FromUpnpCookie(void *cookie) noexcept {
		return *(UpnpCallback *)cookie;
	}

	virtual int Invoke(Upnp_EventType et, const void *evp) noexcept = 0;
};

#endif
