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

#include "UniqueTags.hxx"
#include "Interface.hxx"
#include "song/LightSong.hxx"
#include "tag/VisitFallback.hxx"
#include "util/ConstBuffer.hxx"
#include "util/RecursiveMap.hxx"

static void
CollectUniqueTags(RecursiveMap<std::string> &result,
		  const Tag &tag,
		  ConstBuffer<TagType> tag_types) noexcept
{
	if (tag_types.empty())
		return;

	const auto tag_type = tag_types.shift();

	VisitTagWithFallbackOrEmpty(tag, tag_type, [&result, &tag, tag_types](const char *value){
			CollectUniqueTags(result[value], tag, tag_types);
		});
}

RecursiveMap<std::string>
CollectUniqueTags(const Database &db, const DatabaseSelection &selection,
		  ConstBuffer<TagType> tag_types)
{
	RecursiveMap<std::string> result;

	db.Visit(selection, [&result, tag_types](const LightSong &song){
			CollectUniqueTags(result, song.tag, tag_types);
		});

	return result;
}
