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

#ifndef MPD_UPNP_OBJECT_HXX
#define MPD_UPNP_OBJECT_HXX

#include <string>
#include <map>

/**
 * UpnP Media Server directory entry, converted from XML data.
 *
 * This is a dumb data holder class, a struct with helpers.
 */
class UPnPDirObject {
public:
	enum ObjType {item, container};
	// There are actually several kinds of containers:
	// object.container.storageFolder, object.container.person,
	// object.container.playlistContainer etc., but they all seem to
	// behave the same as far as we're concerned. Otoh, musicTrack
	// items are special to us, and so should playlists, but I've not
	// seen one of the latter yet (servers seem to use containers for
	// playlists).
	enum ItemClass {audioItem_musicTrack, audioItem_playlist};

	std::string m_id; // ObjectId
	std::string m_pid; // Parent ObjectId
	std::string m_title; // dc:title. Directory name for a container.
	ObjType m_type; // item or container
	ItemClass m_iclass;
	// Properties as gathered from the XML document (url, artist, etc.)
	// The map keys are the XML tag or attribute names.
	std::map<std::string, std::string> m_props;

	/** Get named property
	 * @param property name (e.g. upnp:artist, upnp:album,
	 *     upnp:originalTrackNumber, upnp:genre). Use m_title instead
	 *     for dc:title.
	 * @param[out] value
	 * @return true if found.
	 */
	const char *getprop(const char *name) const {
		auto it = m_props.find(name);
		if (it == m_props.end())
			return nullptr;
		return it->second.c_str();
	}

	void clear()
	{
		m_id.clear();
		m_pid.clear();
		m_title.clear();
		m_type = (ObjType)-1;
		m_iclass = (ItemClass)-1;
		m_props.clear();
	}
};

#endif /* _UPNPDIRCONTENT_H_X_INCLUDED_ */
