// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "UniqueTags.hxx"
#include "Interface.hxx"
#include "song/LightSong.hxx"
#include "tag/VisitFallback.hxx"
#include "util/RecursiveMap.hxx"

static void
CollectUniqueTags(RecursiveMap<std::string> &result,
		  const Tag &tag,
		  std::span<const TagType> tag_types) noexcept
{
	if (tag_types.empty())
		return;

	const auto tag_type = tag_types.front();
	tag_types = tag_types.subspan(1);

	VisitTagWithFallbackOrEmpty(tag, tag_type, [&result, &tag, tag_types](const char *value){
			CollectUniqueTags(result[value], tag, tag_types);
		});
}

RecursiveMap<std::string>
CollectUniqueTags(const Database &db, const DatabaseSelection &selection,
		  std::span<const TagType> tag_types)
{
	RecursiveMap<std::string> result;

	db.Visit(selection, [&result, tag_types](const LightSong &song){
			CollectUniqueTags(result, song.tag, tag_types);
		});

	return result;
}
