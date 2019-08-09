/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "ContentDirectoryService.hxx"
#include "UniqueIxml.hxx"
#include "Device.hxx"
#include "ixmlwrap.hxx"
#include "util/UriRelative.hxx"
#include "util/RuntimeError.hxx"
#include "util/SplitString.hxx"

#include <upnptools.h>

ContentDirectoryService::ContentDirectoryService(const UPnPDevice &device,
						 const UPnPService &service) noexcept
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

ContentDirectoryService::~ContentDirectoryService() noexcept
{
	/* this destructor exists here just so it won't get inlined */
}

std::forward_list<std::string>
ContentDirectoryService::getSearchCapabilities(UpnpClient_Handle hdl) const
{
	UniqueIxmlDocument request(UpnpMakeAction("GetSearchCapabilities", m_serviceType.c_str(),
						  0,
						  nullptr, nullptr));
	if (!request)
		throw std::runtime_error("UpnpMakeAction() failed");

	IXML_Document *_response;
	auto code = UpnpSendAction(hdl, m_actionURL.c_str(),
				   m_serviceType.c_str(),
				   0 /*devUDN*/, request.get(), &_response);
	if (code != UPNP_E_SUCCESS)
		throw FormatRuntimeError("UpnpSendAction() failed: %s",
					 UpnpGetErrorMessage(code));

	UniqueIxmlDocument response(_response);

	const char *s = ixmlwrap::getFirstElementValue(response.get(),
						       "SearchCaps");
	if (s == nullptr || *s == 0)
		/* we could just "return {}" here, but GCC 5 doesn't
		   understand that */
		return std::forward_list<std::string>();

	return SplitString(s, ',', false);
}
