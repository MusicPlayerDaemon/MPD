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

#include <string>
#include <vector>

#include <string.h>

static const char *const upnptags[] = {
	"upnp:artist",
	"upnp:album",
	"upnp:genre",
	"upnp:originalTrackNumber",
	"upnp:class",
	nullptr,
};

static const char *const res_attributes[] = {
	"protocolInfo",
	"size",
	"bitrate",
	"duration",
	"sampleFrequency",
	"nrAudioChannels",
	nullptr,
};

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

/**
 * An XML parser which builds directory contents from DIDL lite input.
 */
class UPnPDirParser final : public CommonExpatParser {
	std::vector<std::string> m_path;
	UPnPDirObject m_tobj;

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

				for (auto i = res_attributes; *i != nullptr; ++i) {
					const char *value = GetAttribute(attrs, *i);
					if (value != nullptr)
						m_tobj.m_props.emplace(*i, value);
				}
			}

			break;
		}
	}

	bool checkobjok() {
		bool ok =  !m_tobj.m_id.empty() && !m_tobj.m_pid.empty() &&
			!m_tobj.m_title.empty();

		if (ok && m_tobj.type == UPnPDirObject::Type::ITEM) {
			const char *item_class_name =
				m_tobj.m_props["upnp:class"].c_str();
			auto item_class = ParseItemClass(item_class_name);
			if (item_class == UPnPDirObject::ItemClass::UNKNOWN) {
				ok = false;
			} else {
				m_tobj.item_class = item_class;
			}
		}

		return ok;
	}

	virtual void EndElement(const XML_Char *name)
	{
		if (!strcmp(name, "container")) {
			if (checkobjok()) {
				m_dir.m_containers.push_back(m_tobj);
			}
		} else if (!strcmp(name, "item")) {
			if (checkobjok()) {
				m_dir.m_items.push_back(m_tobj);
			}
		}

		m_path.pop_back();
	}

	virtual void CharacterData(const XML_Char *s, int len)
	{
		if (s == 0 || *s == 0)
			return;
		std::string str(s, len);
		trimstring(str);
		switch (m_path.back()[0]) {
		case 'd':
			if (!m_path.back().compare("dc:title"))
				m_tobj.m_title += str;
			break;
		case 'r':
			if (!m_path.back().compare("res")) {
				m_tobj.m_props["url"] += str;
			}
			break;
		case 'u':
			for (auto i = upnptags; *i != nullptr; ++i)
				if (!m_path.back().compare(*i))
					m_tobj.m_props[*i] += str;
			break;
		}
	}
};

bool
UPnPDirContent::parse(const std::string &input, Error &error)
{
	UPnPDirParser parser(*this);
	return parser.Parse(input.data(), input.length(), true, error);
}
