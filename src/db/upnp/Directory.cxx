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
#include <map>

#include <string.h>

static const char *const upnptags[] = {
	"upnp:artist",
	"upnp:album",
	"upnp:genre",
	"upnp:originalTrackNumber",
	"upnp:class",
};
static const int nupnptags = sizeof(upnptags) / sizeof(char*);

/**
 * An XML parser which builds directory contents from DIDL lite input.
 */
class UPnPDirParser final : public CommonExpatParser {
	struct StackEl {
		StackEl(const std::string& nm) : name(nm) {}
		std::string name;
		std::map<std::string,std::string> attributes;
	};

	std::vector<StackEl> m_path;
	UPnPDirObject m_tobj;
	std::map<std::string, UPnPDirObject::ItemClass> m_okitems;

public:
	UPnPDirParser(UPnPDirContent& dir)
		:m_dir(dir)
	{
		m_okitems["object.item.audioItem.musicTrack"] =
			UPnPDirObject::ItemClass::MUSIC;
		m_okitems["object.item.playlistItem"] =
			UPnPDirObject::ItemClass::PLAYLIST;
	}
	UPnPDirContent& m_dir;

protected:
	virtual void StartElement(const XML_Char *name, const XML_Char **attrs)
	{
		m_path.push_back(StackEl(name));
		for (int i = 0; attrs[i] != 0; i += 2) {
			m_path.back().attributes[attrs[i]] = attrs[i+1];
		}

		switch (name[0]) {
		case 'c':
			if (!strcmp(name, "container")) {
				m_tobj.clear();
				m_tobj.type = UPnPDirObject::Type::CONTAINER;
				m_tobj.m_id = m_path.back().attributes["id"];
				m_tobj.m_pid = m_path.back().attributes["parentID"];
			}
			break;
		case 'i':
			if (!strcmp(name, "item")) {
				m_tobj.clear();
				m_tobj.type = UPnPDirObject::Type::ITEM;
				m_tobj.m_id = m_path.back().attributes["id"];
				m_tobj.m_pid = m_path.back().attributes["parentID"];
			}
			break;
		default:
			break;
		}
	}

	bool checkobjok() {
		bool ok =  !m_tobj.m_id.empty() && !m_tobj.m_pid.empty() &&
			!m_tobj.m_title.empty();

		if (ok && m_tobj.type == UPnPDirObject::Type::ITEM) {
			auto it = m_okitems.find(m_tobj.m_props["upnp:class"]);
			if (it == m_okitems.end()) {
				PLOGINF("checkobjok: found object of unknown class: [%s]\n",
					m_tobj.m_props["upnp:class"].c_str());
				ok = false;
			} else {
				m_tobj.item_class = it->second;
			}
		}

		if (!ok) {
			PLOGINF("checkobjok: skip: id [%s] pid [%s] clss [%s] tt [%s]\n",
				m_tobj.m_id.c_str(), m_tobj.m_pid.c_str(),
				m_tobj.m_props["upnp:class"].c_str(),
				m_tobj.m_title.c_str());
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
		} else if (!strcmp(name, "res")) {
			// <res protocolInfo="http-get:*:audio/mpeg:*" size="5171496"
			// bitrate="24576" duration="00:03:35" sampleFrequency="44100"
			// nrAudioChannels="2">
			std::string s;
			s="protocolInfo";m_tobj.m_props[s] = m_path.back().attributes[s];
			s="size";m_tobj.m_props[s] = m_path.back().attributes[s];
			s="bitrate";m_tobj.m_props[s] = m_path.back().attributes[s];
			s="duration";m_tobj.m_props[s] = m_path.back().attributes[s];
			s="sampleFrequency";m_tobj.m_props[s] = m_path.back().attributes[s];
			s="nrAudioChannels";m_tobj.m_props[s] = m_path.back().attributes[s];
		}

		m_path.pop_back();
	}

	virtual void CharacterData(const XML_Char *s, int len)
	{
		if (s == 0 || *s == 0)
			return;
		std::string str(s, len);
		trimstring(str);
		switch (m_path.back().name[0]) {
		case 'd':
			if (!m_path.back().name.compare("dc:title"))
				m_tobj.m_title += str;
			break;
		case 'r':
			if (!m_path.back().name.compare("res")) {
				m_tobj.m_props["url"] += str;
			}
			break;
		case 'u':
			for (int i = 0; i < nupnptags; i++) {
				if (!m_path.back().name.compare(upnptags[i])) {
					m_tobj.m_props[upnptags[i]] += str;
				}
			}
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
