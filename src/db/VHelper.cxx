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

#include "VHelper.hxx"
#include "song/DetachedSong.hxx"
#include "song/LightSong.hxx"
#include "song/Filter.hxx"
#include "tag/Sort.hxx"

#include <algorithm>
#include <cassert>
#include <utility>

DatabaseVisitorHelper::DatabaseVisitorHelper(DatabaseSelection _selection,
					     VisitSong &visit_song) noexcept
	:selection(std::move(_selection))
{
	// TODO: apply URI and SongFilter
	assert(selection.uri.empty());
	assert(selection.filter == nullptr);

	if (selection.sort != TAG_NUM_OF_ITEM_TYPES) {
		/* the client has asked us to sort the result; this is
		   pretty expensive, because instead of streaming the
		   result to the client, we need to copy it all into
		   this std::vector, and then sort it */

		original_visit_song = std::move(visit_song);
		visit_song = [this](const auto &song){
			songs.emplace_back(song);
		};
	} else if (selection.window != RangeArg::All()) {
		original_visit_song = std::move(visit_song);
		visit_song = [this](const auto &song){
			if (selection.window.Contains(counter++))
				original_visit_song(song);
		};
	}
}

DatabaseVisitorHelper::~DatabaseVisitorHelper() noexcept = default;

void
DatabaseVisitorHelper::Commit()
{
	/* only needed if sorting is enabled */
	if (selection.sort == TAG_NUM_OF_ITEM_TYPES)
		return;

	assert(original_visit_song);

	/* sort the song collection */
	const auto sort = selection.sort;
	const auto descending = selection.descending;

	if (sort == TagType(SORT_TAG_LAST_MODIFIED))
		std::stable_sort(songs.begin(), songs.end(),
				 [descending](const DetachedSong &a, const DetachedSong &b){
					 return descending
						 ? a.GetLastModified() > b.GetLastModified()
						 : a.GetLastModified() < b.GetLastModified();
				 });
	else
		std::stable_sort(songs.begin(), songs.end(),
				 [sort, descending](const DetachedSong &a,
						    const DetachedSong &b){
					 return CompareTags(sort, descending,
							    a.GetTag(),
							    b.GetTag());
				 });

	/* apply the "window" */
	if (selection.window.end < songs.size())
		songs.erase(std::next(songs.begin(), selection.window.end),
			    songs.end());

	if (selection.window.start >= songs.size())
		return;

	songs.erase(songs.begin(),
		    std::next(songs.begin(), selection.window.start));

	/* now pass all songs to the original visitor callback */
	for (const auto &song : songs)
		original_visit_song((LightSong)song);
}
