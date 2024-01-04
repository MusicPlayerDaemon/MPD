// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <vector>
#include <string>

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
	std::string controlURL; // e.g.: /upnp/control/cm
};

/**
 * Data holder for a UPnP device, parsed from the XML description obtained
 * during discovery.
 * A device may include several services. To be of interest to us,
 * one of them must be a ContentDirectory.
 */
class UPnPDevice {
public:
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

	UPnPDevice() = default;
	UPnPDevice(const UPnPDevice &) = delete;
	UPnPDevice(UPnPDevice &&) = default;
	UPnPDevice &operator=(UPnPDevice &&) = default;

	~UPnPDevice() noexcept;

	/** Build device from xml description downloaded from discovery
	 * @param url where the description came from
	 * @param description the xml device description
	 */
	void Parse(std::string_view url, std::string_view description);
};
