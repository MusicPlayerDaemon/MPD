// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Object.hxx"

#include <string_view>
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
	void Parse(std::string_view didltext);
};
