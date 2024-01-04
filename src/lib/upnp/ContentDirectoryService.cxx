// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ContentDirectoryService.hxx"
#include "Action.hxx"
#include "Device.hxx"
#include "util/IterableSplitString.hxx"
#include "util/UriRelative.hxx"
#include "util/UriUtil.hxx"
#include "config.h"

using std::string_view_literals::operator""sv;

ContentDirectoryService::ContentDirectoryService(const UPnPDevice &device,
						 const UPnPService &service) noexcept
	:m_actionURL(uri_apply_base(service.controlURL, device.URLBase)),
	 m_serviceType(service.serviceType),
	 m_deviceId(device.UDN),
	 m_friendlyName(device.friendlyName),
	 m_rdreqcnt(200)
{
	if (device.modelName == "MediaTomb"sv) {
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
	const auto response = UpnpSendAction(hdl, m_actionURL.c_str(),
					     "GetSearchCapabilities", m_serviceType.c_str(),
					     {});

	const char *s = response.GetValue("SearchCaps");
	if (s == nullptr || *s == 0)
		return {};

	std::forward_list<std::string> result;
	for (const auto &i : IterableSplitString(s, ','))
		result.emplace_front(i);
	return result;
}
