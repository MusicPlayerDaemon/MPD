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
#include "upnpplib.hxx"
#include "Domain.hxx"
#include "Log.hxx"

#include <upnp/ixml.h>
#include <upnp/upnptools.h>

static LibUPnP *theLib;

LibUPnP *
LibUPnP::getLibUPnP(Error &error)
{
	if (theLib == nullptr)
		theLib = new LibUPnP;

	if (!theLib->ok()) {
		error.Set(theLib->GetInitError());
		return nullptr;
	}

	return theLib;
}

LibUPnP::LibUPnP()
{
	auto code = UpnpInit(0, 0);
	if (code != UPNP_E_SUCCESS) {
		init_error.Format(upnp_domain, code,
				  "UpnpInit() failed: %s",
				  UpnpGetErrorMessage(code));
		return;
	}

	UpnpSetMaxContentLength(2000*1024);

	code = UpnpRegisterClient(o_callback, (void *)this, &m_clh);
	if (code != UPNP_E_SUCCESS) {
		init_error.Format(upnp_domain, code,
				  "UpnpRegisterClient() failed: %s",
				  UpnpGetErrorMessage(code));
		return;
	}

	// Servers sometimes make error (e.g.: minidlna returns bad utf-8)
	ixmlRelaxParser(1);
}

void
LibUPnP::registerHandler(Upnp_EventType et, Upnp_FunPtr handler, void *cookie)
{
	if (handler == nullptr)
		m_handlers.erase(et);
	else
		m_handlers.emplace(et, Handler(handler, cookie));
}

int
LibUPnP::o_callback(Upnp_EventType et, void* evp, void* cookie)
{
	LibUPnP *ulib = (LibUPnP *)cookie;
	if (ulib == nullptr) {
		// Because the asyncsearch calls uses a null cookie.
		ulib = theLib;
	}

	auto it = ulib->m_handlers.find(et);
	if (it != ulib->m_handlers.end()) {
		(it->second.handler)(et, evp, it->second.cookie);
	}
	return UPNP_E_SUCCESS;
}

LibUPnP::~LibUPnP()
{
	int error = UpnpFinish();
	if (error != UPNP_E_SUCCESS)
		FormatError(upnp_domain, "UpnpFinish() failed: %s",
			    UpnpGetErrorMessage(error));
}
