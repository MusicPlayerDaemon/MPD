// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
