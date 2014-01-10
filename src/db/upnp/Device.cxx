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
#include "Log.hxx"
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
			m_device.services.push_back(m_tservice);
			m_tservice.clear();
		}

		m_path.pop_back();
	}

	virtual void CharacterData(const XML_Char *s, int len) {
		std::string str(s, len);
		trimstring(str);
		switch (m_path.back()[0]) {
		case 'c':
			if (!m_path.back().compare("controlURL"))
				m_tservice.controlURL += str;
			break;
		case 'd':
			if (!m_path.back().compare("deviceType"))
				m_device.deviceType += str;
			break;
		case 'e':
			if (!m_path.back().compare("eventSubURL"))
				m_tservice.eventSubURL += str;
			break;
		case 'f':
			if (!m_path.back().compare("friendlyName"))
				m_device.friendlyName += str;
			break;
		case 'm':
			if (!m_path.back().compare("manufacturer"))
				m_device.manufacturer += str;
			else if (!m_path.back().compare("modelName"))
				m_device.modelName += str;
			break;
		case 's':
			if (!m_path.back().compare("serviceType"))
				m_tservice.serviceType = str;
			else if (!m_path.back().compare("serviceId"))
				m_tservice.serviceId += str;
		case 'S':
			if (!m_path.back().compare("SCPDURL"))
				m_tservice.SCPDURL = str;
			break;
		case 'U':
			if (!m_path.back().compare("UDN"))
				m_device.UDN = str;
			else if (!m_path.back().compare("URLBase"))
				m_device.URLBase += str;
			break;
		}
	}
};

UPnPDevice::UPnPDevice(const std::string &url, const std::string &description)
	:ok(false)
{
	UPnPDeviceParser mparser(*this);
	Error error;
	if (!mparser.Parse(description.data(), description.length(), true,
			   error)) {
		// TODO: pass Error to caller
		LogError(error);
		return;
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
	ok = true;
}
