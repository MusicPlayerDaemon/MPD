// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "util/IntrusiveList.hxx"

#include <upnp.h>

#include <cstdint>

/**
 * A class that is supposed to be used for libupnp asynchronous
 * callbacks.
 *
 * libupnp cannot cancel pending callbacks, therefore the cookie is
 * not a pointer to this object, but a unique number which is looked
 * up in a global registry by Dispatch().  The object is registered by
 * the first GetUpnpCookie() call.  The derived class must call
 * UnregisterUpnpCallback() at the beginning of its destructor.
 */
class UpnpCallback {
	friend struct UpnpCallbackRegistryHook;

	/**
	 * The hook for the global registry.  Protected by the
	 * #registry_mutex.
	 */
	IntrusiveListHook<IntrusiveHookMode::TRACK> registry_siblings;

	/**
	 * A unique number (never 0, never reused) which is passed as
	 * cookie to libupnp.  Assigned by the first GetUpnpCookie()
	 * call.
	 */
	std::uintptr_t cookie_id;

	/**
	 * The number of Invoke() calls currently running.  Protected
	 * by the registry mutex.
	 */
	unsigned n_invoking = 0;

#ifndef NDEBUG
	/**
	 * Was UnregisterUpnpCallback() called already?  After that,
	 * GetUpnpCookie() must not be called (it would register the
	 * object again).  Protected by the registry mutex.
	 */
	bool unregistered = false;
#endif

protected:
	UpnpCallback() noexcept = default;

#ifndef NDEBUG
	~UpnpCallback() noexcept;
#endif

	UpnpCallback(const UpnpCallback &) = delete;
	UpnpCallback &operator=(const UpnpCallback &) = delete;

public:
	/**
	 * Pass this value as "cookie" pointer to libupnp asynchronous
	 * functions.  The first call registers this object.
	 */
	void *GetUpnpCookie() noexcept;

	/**
	 * Invoke the object identified by the given cookie, unless it
	 * has been unregistered already.
	 */
	static int Dispatch(void *cookie,
			    Upnp_EventType et, const void *evp) noexcept;

protected:
	/**
	 * Unregister this object and wait for running Invoke() calls
	 * to return; after that, Invoke() will not be called again,
	 * even if libupnp still has pending callbacks with our
	 * cookie.  Must not be called from within Invoke().
	 */
	void UnregisterUpnpCallback() noexcept;

	virtual int Invoke(Upnp_EventType et, const void *evp) noexcept = 0;
};
