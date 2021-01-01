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

#include "TagSongFilter.hxx"
#include "Escape.hxx"
#include "LightSong.hxx"
#include "tag/Tag.hxx"
#include "tag/Fallback.hxx"

std::string
TagSongFilter::ToExpression() const noexcept
{
	const char *name = type == TAG_NUM_OF_ITEM_TYPES
		? "any"
		: tag_item_names[type];

	return std::string("(") + name + " " + filter.GetOperator()
		+ " \"" + EscapeFilterString(filter.GetValue()) + "\")";
}

bool
TagSongFilter::Match(const Tag &tag) const noexcept
{
	bool visited_types[TAG_NUM_OF_ITEM_TYPES]{};

	for (const auto &i : tag) {
		visited_types[i.type] = true;

		if ((type == TAG_NUM_OF_ITEM_TYPES || i.type == type) &&
		    filter.MatchWithoutNegation(i.value))
			return !filter.IsNegated();
	}

	if (type < TAG_NUM_OF_ITEM_TYPES && !visited_types[type]) {
		/* if the specified tag is not present, try the
		   fallback tags */

		bool result = false;
		if (ApplyTagFallback(type, [&](TagType tag2) {
			if (!visited_types[tag2])
				/* we already know that this tag type
				   isn't present, so let's bail out
				   without checking again */
				return false;

			for (const auto &item : tag) {
				if (item.type == tag2 &&
				    filter.MatchWithoutNegation(item.value)) {
					result = true;
					break;
				}
			}

			return true;
		}))
			return result != filter.IsNegated();

		/* If the search critieron was not visited during the
		   sweep through the song's tag, it means this field
		   is absent from the tag or empty. Thus, if the
		   searched string is also empty
		   then it's a match as well and we should return
		   true. */
		if (filter.empty())
			return !filter.IsNegated();
	}

	return filter.IsNegated();
}

bool
TagSongFilter::Match(const LightSong &song) const noexcept
{
	return Match(song.tag);
}
