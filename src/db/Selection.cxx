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

#include "config.h"
#include "Selection.hxx"
#include "SongFilter.hxx"
#include <string.h>
#include <limits>

DatabaseSelection::DatabaseSelection(const char *_uri, bool _recursive,
				     const SongFilter *_filter)
	:uri(_uri), recursive(_recursive), ignore_repeat(false), update(false),
	window_start(0), window_end(std::numeric_limits<unsigned>::max()), filter(_filter)
{
	/* optimization: if the caller didn't specify a base URI, pick
	   the one from SongFilter */
	if (uri.empty() && filter != nullptr) {
		auto base = filter->GetBase();
		if (base != nullptr)
			uri = base;
	}
}

DatabaseSelection::DatabaseSelection(const char *_uri, bool _recursive,
				     bool _ignore_repeat, const SongFilter *_filter)
	:uri(_uri), recursive(_recursive), ignore_repeat(_ignore_repeat), update(false),
	window_start(0), window_end(std::numeric_limits<unsigned>::max()), filter(_filter)
{
	/* optimization: if the caller didn't specify a base URI, pick
	   the one from SongFilter */
	if (uri.empty() && filter != nullptr) {
		auto base = filter->GetBase();
		if (base != nullptr)
			uri = base;
	}
}

DatabaseSelection::DatabaseSelection(const char *_uri, bool _recursive,
				     bool _ignore_repeat, bool _update, const SongFilter *_filter)
	:uri(_uri), recursive(_recursive), ignore_repeat(_ignore_repeat), update(_update),
	window_start(0), window_end(std::numeric_limits<unsigned>::max()), filter(_filter)
{
	/* optimization: if the caller didn't specify a base URI, pick
	   the one from SongFilter */
	if (uri.empty() && filter != nullptr) {
		auto base = filter->GetBase();
		if (base != nullptr)
			uri = base;
	}
}

bool
DatabaseSelection::IsEmpty() const noexcept
{
	return uri.empty() && (filter == nullptr || filter->IsEmpty());
}

bool
DatabaseSelection::HasOtherThanBase() const noexcept
{
	return filter != nullptr && filter->HasOtherThanBase();
}

bool
DatabaseSelection::Match(const LightSong &song) const noexcept
{
	return filter == nullptr || filter->Match(song);
}
