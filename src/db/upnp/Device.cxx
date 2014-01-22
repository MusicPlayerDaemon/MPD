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
#include "Device.hxx"
#include "Util.hxx"
#include "Expat.hxx"
#include "util/Error.hxx"

#include <stdlib.h>

#include <string.h>

/**
 * An XML parser which constructs an UPnP device object from the
 * device descriptor.
 */
class UPnPDeviceParser final : public CommonExpatParser {
	UPnPDevice &m_device;
	std::vector<std::string> m_path;
	UPnPService m_tservice;

public:
	UPnPDeviceParser(UPnPDevice& device)
		:m_device(device) {}

protected:
	virtual void StartElement(const XML_Char *name, const XML_Char **) {
		m_path.push_back(name);
	}

	virtual void EndElement(const XML_Char *name) {
		if (!strcmp(name, "service")) {
			m_device.services.emplace_back(std::move(m_tservice));
			m_tservice.clear();
		}

		m_path.pop_back();
	}

	virtual void CharacterData(const XML_Char *s, int len) {
		const auto &current = m_path.back();
		std::string str = trimstring(s, len);
		switch (current[0]) {
		case 'c':
			if (!current.compare("controlURL"))
				m_tservice.controlURL = std::move(str);
			break;
		case 'd':
			if (!current.compare("deviceType"))
				m_device.deviceType = std::move(str);
			break;
		case 'f':
			if (!current.compare("friendlyName"))
				m_device.friendlyName = std::move(str);
			break;
		case 'm':
			if (!current.compare("manufacturer"))
				m_device.manufacturer = std::move(str);
			else if (!current.compare("modelName"))
				m_device.modelName = std::move(str);
			break;
		case 's':
			if (!current.compare("serviceType"))
				m_tservice.serviceType = std::move(str);
			break;
		case 'U':
			if (!current.compare("UDN"))
				m_device.UDN = std::move(str);
			else if (!current.compare("URLBase"))
				m_device.URLBase = std::move(str);
			break;
		}
	}
};

bool
UPnPDevice::Parse(const std::string &url, const char *description,
		  Error &error)
{
	{
		UPnPDeviceParser mparser(*this);
		if (!mparser.Parse(description, strlen(description),
				   true, error))
			return false;
	}

	if (URLBase.empty()) {
		// The standard says that if the URLBase value is empty, we should use
		// the url the description was retrieved from. However this is
		// sometimes something like http://host/desc.xml, sometimes something
		// like http://host/

		if (url.size() < 8) {
			// ???
			URLBase = url;
		} else {
			auto hostslash = url.find_first_of("/", 7);
			if (hostslash == std::string::npos || hostslash == url.size()-1) {
				URLBase = url;
			} else {
				URLBase = path_getfather(url);
			}
		}
	}

	return true;
}
