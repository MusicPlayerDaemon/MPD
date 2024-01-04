// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Device.hxx"
#include "Util.hxx"
#include "lib/expat/ExpatParser.hxx"

#include <string.h>

/**
 * An XML parser which constructs an UPnP device object from the
 * device descriptor.
 */
class UPnPDeviceParser final : public CommonExpatParser {
	UPnPDevice &m_device;

	std::string *value;

	UPnPService m_tservice;

public:
	explicit UPnPDeviceParser(UPnPDevice& device)
		:m_device(device),
		 value(nullptr) {}

protected:
	void StartElement(const XML_Char *name, const XML_Char **) override {
		value = nullptr;

		switch (name[0]) {
		case 'c':
			if (strcmp(name, "controlURL") == 0)
				value = &m_tservice.controlURL;
			break;
		case 'd':
			if (strcmp(name, "deviceType") == 0)
				value = &m_device.deviceType;
			break;
		case 'f':
			if (strcmp(name, "friendlyName") == 0)
				value = &m_device.friendlyName;
			break;
		case 'm':
			if (strcmp(name, "manufacturer") == 0)
				value = &m_device.manufacturer;
			else if (strcmp(name, "modelName") == 0)
				value = &m_device.modelName;
			break;
		case 's':
			if (strcmp(name, "serviceType") == 0)
				value = &m_tservice.serviceType;
			break;
		case 'U':
			if (strcmp(name, "UDN") == 0)
				value = &m_device.UDN;
			else if (strcmp(name, "URLBase") == 0)
				value = &m_device.URLBase;
			break;
		}
	}

	void EndElement(const XML_Char *name) override {
		if (value != nullptr) {
			trimstring(*value);
			value = nullptr;
		} else if (!strcmp(name, "service")) {
			m_device.services.emplace_front(std::move(m_tservice));
			m_tservice = {};
		}
	}

	void CharacterData(std::string_view s) override {
		if (value != nullptr)
			value->append(s);
	}
};

void
UPnPDevice::Parse(const std::string_view url, const std::string_view description)
{
	{
		UPnPDeviceParser mparser(*this);
		mparser.Parse(description, true);
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
			auto hostslash = url.find('/', 7);
			if (hostslash == std::string::npos || hostslash == url.size()-1) {
				URLBase = url;
			} else {
				URLBase = path_getfather(url);
			}
		}
	}
}
