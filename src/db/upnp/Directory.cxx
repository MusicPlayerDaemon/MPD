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
#include "Directory.hxx"
#include "Util.hxx"
#include "Expat.hxx"
#include "Tags.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/TagTable.hxx"

#include <algorithm>
#include <string>
#include <vector>

#include <string.h>

gcc_pure
static UPnPDirObject::ItemClass
ParseItemClass(const char *name)
{
	if (strcmp(name, "object.item.audioItem.musicTrack") == 0)
		return UPnPDirObject::ItemClass::MUSIC;
	else if (strcmp(name, "object.item.playlistItem") == 0)
		return UPnPDirObject::ItemClass::PLAYLIST;
	else
		return UPnPDirObject::ItemClass::UNKNOWN;
}

gcc_pure
static int
ParseDuration(const std::string &duration)
{
	const auto v = stringToTokens(duration, ":");
	if (v.size() != 3)
		return 0;
	return atoi(v[0].c_str())*3600 + atoi(v[1].c_str())*60 + atoi(v[2].c_str());
}

/**
 * Transform titles to turn '/' into '_' to make them acceptable path
 * elements. There is a very slight risk of collision in doing
 * this. Twonky returns directory names (titles) like 'Artist/Album'.
 */
gcc_pure
static std::string
titleToPathElt(std::string &&s)
{
	std::replace(s.begin(), s.end(), '/', '_');
	return s;
}

/**
 * An XML parser which builds directory contents from DIDL lite input.
 */
class UPnPDirParser final : public CommonExpatParser {
	std::vector<std::string> m_path;
	UPnPDirObject m_tobj;
	TagBuilder tag;

public:
	UPnPDirParser(UPnPDirContent& dir)
		:m_dir(dir)
	{
	}
	UPnPDirContent& m_dir;

protected:
	virtual void StartElement(const XML_Char *name, const XML_Char **attrs)
	{
		m_path.push_back(name);

		switch (name[0]) {
		case 'c':
			if (!strcmp(name, "container")) {
				m_tobj.clear();
				m_tobj.type = UPnPDirObject::Type::CONTAINER;

				const char *id = GetAttribute(attrs, "id");
				if (id != nullptr)
					m_tobj.m_id = id;

				const char *pid = GetAttribute(attrs, "parentID");
				if (pid != nullptr)
					m_tobj.m_pid = pid;
			}
			break;

		case 'i':
			if (!strcmp(name, "item")) {
				m_tobj.clear();
				m_tobj.type = UPnPDirObject::Type::ITEM;

				const char *id = GetAttribute(attrs, "id");
				if (id != nullptr)
					m_tobj.m_id = id;

				const char *pid = GetAttribute(attrs, "parentID");
				if (pid != nullptr)
					m_tobj.m_pid = pid;
			}
			break;

		case 'r':
			if (!strcmp(name, "res")) {
				// <res protocolInfo="http-get:*:audio/mpeg:*" size="5171496"
				// bitrate="24576" duration="00:03:35" sampleFrequency="44100"
				// nrAudioChannels="2">

				const char *duration =
					GetAttribute(attrs, "duration");
				if (duration != nullptr)
					tag.SetTime(ParseDuration(duration));
			}

			break;
		}
	}

	bool checkobjok() {
		if (m_tobj.m_id.empty() || m_tobj.m_pid.empty() ||
		    m_tobj.name.empty() ||
		    (m_tobj.type == UPnPDirObject::Type::ITEM &&
		     m_tobj.item_class == UPnPDirObject::ItemClass::UNKNOWN))
			return false;

		return true;
	}

	virtual void EndElement(const XML_Char *name)
	{
		if ((!strcmp(name, "container") || !strcmp(name, "item")) &&
		    checkobjok()) {
			tag.Commit(m_tobj.tag);
			m_dir.objects.push_back(std::move(m_tobj));
		}

		m_path.pop_back();
	}

	virtual void CharacterData(const XML_Char *s, int len)
	{
		const auto &current = m_path.back();
		std::string str = trimstring(s, len);

		TagType type = tag_table_lookup(upnp_tags,
						current.c_str());
		if (type != TAG_NUM_OF_ITEM_TYPES) {
			tag.AddItem(type, str.c_str());

			if (type == TAG_TITLE)
				m_tobj.name = titleToPathElt(std::move(str));

			return;
		}

		switch (current[0]) {
		case 'r':
			if (!current.compare("res")) {
				m_tobj.url = std::move(str);
			}
			break;
		case 'u':
			if (current == "upnp:class") {
				m_tobj.item_class = ParseItemClass(str.c_str());
				break;
			}
			break;
		}
	}
};

bool
UPnPDirContent::parse(const char *input, Error &error)
{
	UPnPDirParser parser(*this);
	return parser.Parse(input, strlen(input), true, error);
}
