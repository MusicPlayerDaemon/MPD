// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "lib/upnp/Device.hxx"

#include <gtest/gtest.h>

#include <string>

using std::string_view_literals::operator""sv;

static constexpr auto description_url = "http://192.168.1.2:8096/desc.xml"sv;

static std::string
MakeDescription(std::string_view url_base, std::string_view control_url)
{
	std::string result = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
)";

	if (!url_base.empty()) {
		result += "<URLBase>";
		result += url_base;
		result += "</URLBase>\n";
	}

	result += R"(<device>
<deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>
<friendlyName>Test</friendlyName>
<UDN>uuid:12345678-1234-1234-1234-123456789abc</UDN>
<serviceList>
<service>
<serviceType>urn:schemas-upnp-org:service:ContentDirectory:1</serviceType>
<controlURL>)";
	result += control_url;
	result += R"(</controlURL>
</service>
</serviceList>
</device>
</root>
)";

	return result;
}

static UPnPDevice
ParseDevice(std::string_view url_base, std::string_view control_url)
{
	UPnPDevice device;
	device.Parse(description_url,
		     MakeDescription(url_base, control_url));
	return device;
}

TEST(UpnpDevice, RelativeControlURL)
{
	const auto device = ParseDevice({}, "/cd/control");
	EXPECT_EQ(device.URLBase, "http://192.168.1.2:8096/");
	ASSERT_FALSE(device.services.empty());
	EXPECT_EQ(device.services.front().controlURL, "/cd/control");
}

TEST(UpnpDevice, URLBase)
{
	const auto device = ParseDevice("http://192.168.1.2:8096/dlna/",
					"cd/control");
	EXPECT_EQ(device.URLBase, "http://192.168.1.2:8096/dlna/");
	EXPECT_FALSE(device.services.empty());
}

TEST(UpnpDevice, ForeignURLBase)
{
	/* URLBase pointing to a different host is ignored */
	const auto device = ParseDevice("http://127.0.0.1:8096/dlna/",
					"cd/control");
	EXPECT_EQ(device.URLBase, "http://192.168.1.2:8096/");
	EXPECT_FALSE(device.services.empty());
}

TEST(UpnpDevice, AbsoluteControlURL)
{
	const auto device = ParseDevice({},
					"http://192.168.1.2:8096/cd/control");
	ASSERT_FALSE(device.services.empty());
	EXPECT_EQ(device.services.front().controlURL,
		  "http://192.168.1.2:8096/cd/control");
}

TEST(UpnpDevice, ForeignControlURL)
{
	/* a service with an absolute control URL pointing to a
	   different host is ignored */
	EXPECT_TRUE(ParseDevice({}, "http://127.0.0.1:8096/cd/control").services.empty());
	EXPECT_TRUE(ParseDevice({}, "http://192.168.1.2:8097/cd/control").services.empty());
	EXPECT_TRUE(ParseDevice({}, "https://192.168.1.2:8096/cd/control").services.empty());
}
