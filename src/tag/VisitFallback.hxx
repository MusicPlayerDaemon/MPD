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

#ifndef MPD_TAG_VISIT_FALLBACK_HXX
#define MPD_TAG_VISIT_FALLBACK_HXX

#include "Fallback.hxx"
#include "Tag.hxx"

template<typename F>
bool
VisitTagType(const Tag &tag, TagType type, F &&f) noexcept
{
	bool found = false;

	for (const auto &item : tag) {
		if (item.type == type) {
			found = true;
			f(item.value);
		}
	}

	return found;
}

template<typename F>
bool
VisitTagWithFallback(const Tag &tag, TagType type, F &&f) noexcept
{
	return ApplyTagWithFallback(type,
				    [&](TagType type2) {
					    return VisitTagType(tag, type2, f);
				    });
}

template<typename F>
void
VisitTagWithFallbackOrEmpty(const Tag &tag, TagType type, F &&f) noexcept
{
	if (!VisitTagWithFallback(tag, type, f))
		f("");
}

#endif
