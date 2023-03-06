// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Selection.hxx"
#include "song/Filter.hxx"

DatabaseSelection::DatabaseSelection(const char *_uri, bool _recursive,
				     const SongFilter *_filter) noexcept
	:uri(_uri), filter(_filter), recursive(_recursive)
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
DatabaseSelection::IsFiltered() const noexcept
{
	return !uri.empty() || (filter != nullptr && !filter->IsEmpty());
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
