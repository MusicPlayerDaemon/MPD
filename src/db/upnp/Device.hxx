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

#ifndef _UPNPDEV_HXX_INCLUDED_
#define _UPNPDEV_HXX_INCLUDED_

#include <vector>
#include <string>

class Error;

/**
 * UPnP Description phase: interpreting the device description which we
 * downloaded from the URL obtained by the discovery phase.
 */

/**
 * Data holder for a UPnP service, parsed from the XML description
 * downloaded after discovery yielded its URL.
 */
struct UPnPService {
	// e.g. urn:schemas-upnp-org:service:ConnectionManager:1
	std::string serviceType;
	// Unique Id inside device: e.g here THE ConnectionManager
	std::string serviceId; // e.g. urn:upnp-org:serviceId:ConnectionManager
	std::string SCPDURL; // Service description URL. e.g.: cm.xml
	std::string controlURL; // e.g.: /upnp/control/cm
	std::string eventSubURL; // e.g.: /upnp/event/cm

	void clear()
	{
		serviceType.clear();
		serviceId.clear();
		SCPDURL.clear();
		controlURL.clear();
		eventSubURL.clear();
	}
};

/**
 * Data holder for a UPnP device, parsed from the XML description obtained
 * during discovery.
 * A device may include several services. To be of interest to us,
 * one of them must be a ContentDirectory.
 */
class UPnPDevice {
public:
	bool ok;
	// e.g. urn:schemas-upnp-org:device:MediaServer:1
	std::string deviceType;
	// e.g. MediaTomb
	std::string friendlyName;
	// Unique device number. This should match the deviceID in the
	// discovery message. e.g. uuid:a7bdcd12-e6c1-4c7e-b588-3bbc959eda8d
	std::string UDN;
	// Base for all relative URLs. e.g. http://192.168.4.4:49152/
	std::string URLBase;
	// Manufacturer: e.g. D-Link, PacketVideo ("manufacturer")
	std::string manufacturer;
	// Model name: e.g. MediaTomb, DNS-327L ("modelName")
	std::string modelName;
	// Services provided by this device.
	std::vector<UPnPService> services;

	/** Build device from xml description downloaded from discovery
	 * @param url where the description came from
	 * @param description the xml device description
	 */
	UPnPDevice(const std::string &url, const std::string &description);

	UPnPDevice() : ok(false) {}
};

typedef std::vector<UPnPService>::iterator DevServIt;

#endif /* _UPNPDEV_HXX_INCLUDED_ */
