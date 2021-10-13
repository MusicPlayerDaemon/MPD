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

#ifndef MPD_UPNP_DIRECTORY_HXX
#define MPD_UPNP_DIRECTORY_HXX

#include "Object.hxx"

#include <string>
#include <vector>

/**
 * Image of a MediaServer Directory Service container (directory),
 * possibly containing items and subordinate containers.
 */
class UPnPDirContent {
public:
	std::vector<UPnPDirObject> objects;

	UPnPDirContent() = default;
	UPnPDirContent(UPnPDirContent &&) = default;

	~UPnPDirContent();

	[[gnu::pure]]
	UPnPDirObject *FindObject(std::string_view name) noexcept {
		for (auto &o : objects)
			if (o.name == name)
				return &o;

		return nullptr;
	}

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
};

#endif /* _UPNPDIRCONTENT_H_X_INCLUDED_ */
