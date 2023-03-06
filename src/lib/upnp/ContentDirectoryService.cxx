// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"

#include "ContentDirectoryService.hxx"
#include "Device.hxx"
#include "Error.hxx"
#include "UniqueIxml.hxx"
#ifdef USING_PUPNP
#	include "ixmlwrap.hxx"
#endif
#include "Action.hxx"
#include "util/IterableSplitString.hxx"
#include "util/UriRelative.hxx"
#include "util/UriUtil.hxx"

#include <algorithm>

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
	if (m_modelName == "MediaTomb") {
		// Readdir by 200 entries is good for most, but MediaTomb likes
		// them really big. Actually 1000 is better but I don't dare
		m_rdreqcnt = 500;
	}
}

/* this destructor exists here just so it won't get inlined */
ContentDirectoryService::~ContentDirectoryService() noexcept = default;

std::forward_list<std::string>
ContentDirectoryService::getSearchCapabilities(UpnpClient_Handle hdl) const
{
#ifdef USING_PUPNP
	UniqueIxmlDocument request(UpnpMakeAction("GetSearchCapabilities", m_serviceType.c_str(),
						  0,
						  nullptr, nullptr));
	if (!request)
		throw std::runtime_error("UpnpMakeAction() failed");

	IXML_Document *_response;
	auto code = UpnpSendAction(hdl, m_actionURL.c_str(),
				   m_serviceType.c_str(),
				   nullptr /*devUDN*/, request.get(), &_response);
	if (code != UPNP_E_SUCCESS)
		throw Upnp::MakeError(code, "UpnpSendAction() failed");

	UniqueIxmlDocument response(_response);

	const char *s = ixmlwrap::getFirstElementValue(response.get(),
						       "SearchCaps");
#else
	std::vector<std::pair<std::string, std::string>> responseData;
	int errcode;
	std::string errdesc;
	auto code = UpnpSendAction(hdl, "", m_actionURL, m_serviceType,
				   "GetSearchCapabilities", {}, responseData, &errcode,
				   errdesc);
	if (code != UPNP_E_SUCCESS)
		throw Upnp::MakeError(code, "UpnpSendAction() failed");

	const char *s{nullptr};
	for (auto &entry : responseData) {
		if (entry.first == "SearchCaps") {
			s = entry.second.c_str();
		}
	}
#endif
	if (s == nullptr || *s == 0)
		return {};

	std::forward_list<std::string> result;
	for (const auto &i : IterableSplitString(s, ','))
		result.emplace_front(i);
	return result;
}
