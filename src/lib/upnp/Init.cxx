// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Init.hxx"
#include "Error.hxx"
#include "thread/Mutex.hxx"

#include <upnp.h>
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
	if (auto code = UpnpInit2(iface, 0); code != UPNP_E_SUCCESS)
		throw Upnp::MakeError(code, "UpnpInit() failed");

	UpnpSetMaxContentLength(2000*1024);

#ifdef USING_PUPNP
	// Servers sometimes make error (e.g.: minidlna returns bad utf-8)
	ixmlRelaxParser(1);
#endif
}

void
UpnpGlobalInit(const char* iface)
{
	const std::scoped_lock protect{upnp_init_mutex};

	if (upnp_ref == 0)
		DoInit(iface);

	++upnp_ref;
}

void
UpnpGlobalFinish() noexcept
{
	const std::scoped_lock protect{upnp_init_mutex};

	assert(upnp_ref > 0);

	if (--upnp_ref == 0)
		UpnpFinish();
}
