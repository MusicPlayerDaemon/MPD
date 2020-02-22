/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "Compiler.h"
#include "db/Visitor.hxx"

#include <upnp/upnp.h>

#include <string>
#include <list>
#include <vector>

struct DatabaseSelection;
class ContentDirectoryService;

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
		ARTIST,
		ALBUM,
		GENRE,
		MUSIC,
		PLAYLIST,
		FOLDER,
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

	std::vector<UPnPDirObject> childs;

	unsigned total; // total childs match

	UPnPDirObject()
		: id("0"),
		parent_id("0"),
		url(),
		name(),
		type(Type::CONTAINER),
		item_class(ItemClass::UNKNOWN),
		tag(),
		childs(),
		total(std::numeric_limits<unsigned>::max()) {
	}

	UPnPDirObject(const UPnPDirObject &other)
		: id(other.id),
		parent_id(other.parent_id),
		url(other.url),
		name(other.name),
		type(other.type),
		item_class(other.item_class),
		tag(other.tag),
		childs(other.childs),
		total(other.total) {
	}

	UPnPDirObject(UPnPDirObject &&other)
		: id(std::move(other.id)),
		parent_id(std::move(other.parent_id)),
		url(std::move(other.url)),
		name(std::move(other.name)),
		type(other.type),
		item_class(other.item_class),
		tag(other.tag),
		childs(std::move(other.childs)),
		total(other.total) {
	}

	~UPnPDirObject() noexcept;

	void Clear() noexcept {
		id.clear();
		parent_id.clear();
		url.clear();
		type = Type::UNKNOWN;
		item_class = ItemClass::UNKNOWN;
		tag.Clear();
		childs.clear();
		childs.shrink_to_fit();
		total = std::numeric_limits<unsigned>::max();
	}

	gcc_pure
	bool Check() const noexcept {
		return !id.empty() && !parent_id.empty() && !name.empty() &&
			(type != UPnPDirObject::Type::ITEM ||
			 item_class != UPnPDirObject::ItemClass::UNKNOWN);
	}

	gcc_pure
	UPnPDirObject *FindObject(const char *n) {
		for (auto &o : childs)
			if (o.name == n)
				return &o;

		return nullptr;
	}

	gcc_pure
	UPnPDirObject *FindDirectory(const char *n) {
		for (auto &o : childs)
			if (o.name == n &&
				o.type == Type::CONTAINER)
				return &o;

		return nullptr;
	}

	UPnPDirObject *LookupDirectory(std::list<std::string> vpath);

	UPnPDirObject *LookupSong(std::list<std::string> vpath);

	void visitSong(const char *path,
		  const DatabaseSelection &selection,
		  VisitSong visit_song) const;

	void VisitItem(const char *uri,
		  const DatabaseSelection &selection,
		  VisitSong visit_song, VisitPlaylist visit_playlist) const;

	void VisitObject(const char *uri,
			const DatabaseSelection &selection,
			VisitDirectory visit_directory,
			VisitSong visit_song,
			VisitPlaylist visit_playlist) const;

	/**
	 * Parse from DIDL-Lite XML data.
	 *
	 * Normally only used by ContentDirectoryService::readDir()
	 * This is cumulative: in general, the XML data is obtained in
	 * several documents corresponding to (offset,count) slices of the
	 * directory (container). parse() can be called repeatedly with
	 * the successive XML documents and will accumulate entries in the item
	 * and container vectors. This makes more sense if the different
	 * chunks are from the same container, but given that UPnP Ids are
	 * actually global, nothing really bad will happen if you mix
	 * up...
	 */
	void Parse(const char *didltext);

	/**
	 * Caller must lock #db_mutex.
	 */
	void Walk(const char *base_uri, const DatabaseSelection &selection,
		  VisitDirectory visit_directory, VisitSong visit_song,
		  VisitPlaylist visit_playlist) const;

	void Update(ContentDirectoryService &server,
				UpnpClient_Handle handle,
				unsigned window_end = std::numeric_limits<unsigned>::max());

	void Update(ContentDirectoryService &server,
				UpnpClient_Handle handle,
				std::list<std::string> vpath,
				unsigned window_end = std::numeric_limits<unsigned>::max());
};

#endif /* _UPNPDIRCONTENT_H_X_INCLUDED_ */
