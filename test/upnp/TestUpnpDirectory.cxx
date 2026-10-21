// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "db/plugins/upnp/Directory.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

/* Expat delivers entity and character references in separate
   CharacterData() calls; these are used here to check that the
   parser concatenates them */
static constexpr auto didl = R"(<?xml version="1.0"?>
<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">
<container id="1" parentID="0"><dc:title>Music</dc:title><upnp:class>object.container.storageFolder</upnp:class></container>
<item id="2" parentID="1"><dc:title>Song</dc:title><upnp:artist>A &amp; B</upnp:artist><upnp:class>object.item.audioItem&#46;musicTrack</upnp:class><res protocolInfo="http-get:*:audio/mpeg:*">http://192.168.1.2:8096/a?x=1&amp;y=2&amp;z=3</res></item>
</DIDL-Lite>
)"sv;

TEST(UpnpDirectory, Parse)
{
	UPnPDirContent content;
	content.Parse(didl);

	ASSERT_EQ(content.objects.size(), 2U);

	const auto &container = content.objects[0];
	EXPECT_EQ(container.type, UPnPDirObject::Type::CONTAINER);
	EXPECT_EQ(container.id, "1");
	EXPECT_EQ(container.parent_id, "0");
	EXPECT_EQ(container.name, "Music");

	const auto &item = content.objects[1];
	EXPECT_EQ(item.type, UPnPDirObject::Type::ITEM);
	EXPECT_EQ(item.item_class, UPnPDirObject::ItemClass::MUSIC);
	EXPECT_EQ(item.id, "2");
	EXPECT_EQ(item.parent_id, "1");
	EXPECT_EQ(item.name, "Song");
	EXPECT_EQ(item.url, "http://192.168.1.2:8096/a?x=1&y=2&z=3");
	EXPECT_STREQ(item.tag.GetValue(TAG_ARTIST), "A & B");
}
