// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Tags.hxx"
#include "tag/Table.hxx"

const struct tag_table upnp_tags[] = {
	{ "upnp:artist", TAG_ARTIST },
	{ "upnp:album", TAG_ALBUM },
	{ "upnp:originalTrackNumber", TAG_TRACK },
	{ "upnp:genre", TAG_GENRE },
	{ "dc:title", TAG_TITLE },

	/* sentinel */
	{ nullptr, TAG_NUM_OF_ITEM_TYPES }
};
