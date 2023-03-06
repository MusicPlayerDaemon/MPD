// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
