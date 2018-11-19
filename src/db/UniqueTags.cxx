/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

static void
CollectTags(std::set<std::string> &result,
	    const Tag &tag,
	    TagType tag_type) noexcept
{
	VisitTagWithFallbackOrEmpty(tag, tag_type, [&result](const char *value){
			result.emplace(value);
		});
}

static void
CollectGroupTags(std::map<std::string, std::set<std::string>> &result,
		 const Tag &tag,
		 TagType tag_type,
		 TagType group) noexcept
{
	VisitTagWithFallbackOrEmpty(tag, group, [&](const char *group_name){
			CollectTags(result[group_name], tag, tag_type);
		});
}

std::map<std::string, std::set<std::string>>
CollectUniqueTags(const Database &db, const DatabaseSelection &selection,
		  TagType tag_type, TagType group)
{
	std::map<std::string, std::set<std::string>> result;

	db.Visit(selection, [&result, tag_type, group](const LightSong &song){
			CollectGroupTags(result, song.tag, tag_type, group);
		});

	return result;
}
