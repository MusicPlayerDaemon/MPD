// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ClientInit.hxx"
#include "Init.hxx"
#include "Error.hxx"
#include "Callback.hxx"
#include "thread/Mutex.hxx"

#include <upnptools.h>

#include <cassert>

static Mutex upnp_client_init_mutex;
static unsigned upnp_client_ref;
static UpnpClient_Handle upnp_client_handle;

/**
 * Determine the type of the "Event" parameter of #Upnp_FunPtr.  It
 * is "const void *" in most libupnp versions, but "void *" in libupnp
 * 1.18 (and 1.14.26 to 1.14.29), see
 * https://github.com/pupnp/pupnp/issues/528
 */
template<typename F>
struct UpnpFunPtrEvent;

template<typename E>
struct UpnpFunPtrEvent<int(*)(Upnp_EventType, E, void *)> {
	using type = E;
};

static int
UpnpClientCallback(Upnp_EventType et,
		   UpnpFunPtrEvent<Upnp_FunPtr>::type evp,
		   void *cookie) noexcept
{
	if (cookie == nullptr)
		/* this is the cookie passed to UpnpRegisterClient();
		   but can this ever happen?  Will libupnp ever invoke
		   the registered callback without that cookie? */
		return UPNP_E_SUCCESS;

	return UpnpCallback::Dispatch(cookie, et, evp);
}

static void
DoInit()
{
	auto code = UpnpRegisterClient(UpnpClientCallback, nullptr,
				       &upnp_client_handle);
	if (code != UPNP_E_SUCCESS)
		throw Upnp::MakeError(code, "UpnpRegisterClient() failed");
}

UpnpClient_Handle
UpnpClientGlobalInit(const char* iface)
{
	UpnpGlobalInit(iface);

	try {
		const std::lock_guard protect{upnp_client_init_mutex};
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
		const std::lock_guard protect{upnp_client_init_mutex};

		assert(upnp_client_ref > 0);
		if (--upnp_client_ref == 0)
			UpnpUnRegisterClient(upnp_client_handle);
	}

	UpnpGlobalFinish();
}
