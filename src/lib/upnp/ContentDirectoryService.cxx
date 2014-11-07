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
#include "ContentDirectoryService.hxx"
#include "Domain.hxx"
#include "Device.hxx"
#include "ixmlwrap.hxx"
#include "Util.hxx"
#include "Action.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"

ContentDirectoryService::ContentDirectoryService(const UPnPDevice &device,
						 const UPnPService &service)
	:m_actionURL(uri_apply_base(service.controlURL, device.URLBase)),
	 m_serviceType(service.serviceType),
	 m_deviceId(device.UDN),
	 m_friendlyName(device.friendlyName),
	 m_manufacturer(device.manufacturer),
	 m_modelName(device.modelName),
	 m_rdreqcnt(200)
{
	if (!m_modelName.compare("MediaTomb")) {
		// Readdir by 200 entries is good for most, but MediaTomb likes
		// them really big. Actually 1000 is better but I don't dare
		m_rdreqcnt = 500;
	}
}

ContentDirectoryService::~ContentDirectoryService()
{
	/* this destructor exists here just so it won't get inlined */
}

bool
ContentDirectoryService::getSearchCapabilities(UpnpClient_Handle hdl,
					       std::list<std::string> &result,
					       Error &error) const
{
	assert(result.empty());

	IXML_Document *request =
		UpnpMakeAction("GetSearchCapabilities", m_serviceType.c_str(),
			       0,
			       nullptr, nullptr);
	if (request == 0) {
		error.Set(upnp_domain, "UpnpMakeAction() failed");
		return false;
	}

	IXML_Document *response;
	auto code = UpnpSendAction(hdl, m_actionURL.c_str(),
				   m_serviceType.c_str(),
				   0 /*devUDN*/, request, &response);
	ixmlDocument_free(request);
	if (code != UPNP_E_SUCCESS) {
		error.Format(upnp_domain, code,
			     "UpnpSendAction() failed: %s",
			     UpnpGetErrorMessage(code));
		return false;
	}

	const char *s = ixmlwrap::getFirstElementValue(response, "SearchCaps");
	if (s == nullptr || *s == 0) {
		ixmlDocument_free(response);
		return true;
	}

	bool success = true;
	if (!csvToStrings(s, result)) {
		error.Set(upnp_domain, "Bad response");
		success = false;
	}

	ixmlDocument_free(response);
	return success;
}
