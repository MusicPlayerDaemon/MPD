/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "tag/Tag.hxx"

#include <string>

/**
 * UpnP Media Server directory entry, converted from XML data.
 *
 * This is a dumb data holder class, a struct with helpers.
 */
class UPnPDirObject {
public:
	enum class Type {
		UNKNOWN,
		ITEM,
		CONTAINER,
	};

	// There are actually several kinds of containers:
	// object.container.storageFolder, object.container.person,
	// object.container.playlistContainer etc., but they all seem to
	// behave the same as far as we're concerned. Otoh, musicTrack
	// items are special to us, and so should playlists, but I've not
	// seen one of the latter yet (servers seem to use containers for
	// playlists).
	enum class ItemClass {
		UNKNOWN,
		MUSIC,
		PLAYLIST,
	};

	/**
	 * ObjectId
	 */
	std::string id;

	/**
	 * Parent's ObjectId
	 */
	std::string parent_id;

	std::string url;

	/**
	 * A copy of "dc:title" sanitized as a file name.
	 */
	std::string name;

	Type type;
	ItemClass item_class;

	Tag tag;

	UPnPDirObject() = default;
	UPnPDirObject(UPnPDirObject &&) = default;

	~UPnPDirObject() noexcept;

	UPnPDirObject &operator=(UPnPDirObject &&) = default;

	void Clear() noexcept {
		id.clear();
		parent_id.clear();
		url.clear();
		type = Type::UNKNOWN;
		item_class = ItemClass::UNKNOWN;
		tag.Clear();
	}

	[[gnu::pure]]
	bool IsRoot() const noexcept {
		return type == Type::CONTAINER && id == "0";
	}

	[[gnu::pure]]
	bool Check() const noexcept {
		return !id.empty() &&
			/* root nodes don't need a parent id and a
			   name */
			(IsRoot() || (!parent_id.empty() &&
				      !name.empty())) &&
			(type != UPnPDirObject::Type::ITEM ||
			 item_class != UPnPDirObject::ItemClass::UNKNOWN);
	}
};

#endif /* _UPNPDIRCONTENT_H_X_INCLUDED_ */
