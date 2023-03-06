// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
