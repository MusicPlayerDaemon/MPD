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

#include "AndSongFilter.hxx"

#include <algorithm>

ISongFilterPtr
AndSongFilter::Clone() const noexcept
{
	auto result = std::make_unique<AndSongFilter>();

	for (const auto &i : items)
		result->items.emplace_back(i->Clone());

	return result;
}

std::string
AndSongFilter::ToExpression() const noexcept
{
	auto i = items.begin();
	const auto end = items.end();

	if (std::next(i) == end)
		return (*i)->ToExpression();

	std::string e("(");
	e += (*i)->ToExpression();

	for (++i; i != end; ++i) {
		e += " AND ";
		e += (*i)->ToExpression();
	}

	e.push_back(')');
	return e;
}

bool
AndSongFilter::Match(const LightSong &song) const noexcept
{
	return std::all_of(items.begin(), items.end(), [&song](const auto &i) { return i->Match(song); });
}
