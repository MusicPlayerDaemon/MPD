// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
