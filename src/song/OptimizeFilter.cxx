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

#include "OptimizeFilter.hxx"
#include "AndSongFilter.hxx"
#include "NotSongFilter.hxx"
#include "TagSongFilter.hxx"
#include "UriSongFilter.hxx"

void
OptimizeSongFilter(AndSongFilter &af) noexcept
{
	for (auto i = af.items.begin(); i != af.items.end();) {
		auto f = OptimizeSongFilter(std::move(*i));
		if (auto *nested = dynamic_cast<AndSongFilter *>(f.get())) {
			/* collapse nested #AndSongFilter instances */
			af.items.splice(i, std::move(nested->items));
			i = af.items.erase(i);
		} else {
			*i = std::move(f);
			++i;
		}
	}
}

ISongFilterPtr
OptimizeSongFilter(ISongFilterPtr f) noexcept
{
	if (auto *af = dynamic_cast<AndSongFilter *>(f.get())) {
		/* first optimize all items */
		OptimizeSongFilter(*af);

		if (!af->items.empty() &&
		    std::next(af->items.begin()) == af->items.end())
			/* only one item: the containing
			   #AndSongFilter can be removed */
			return std::move(af->items.front());
	} else if (auto *nf = dynamic_cast<NotSongFilter *>(f.get())) {
		auto child = OptimizeSongFilter(std::move(nf->child));
		if (auto *tf = dynamic_cast<TagSongFilter *>(child.get())) {
			/* #TagSongFilter has its own "negated" flag,
			   so we can drop the #NotSongFilter
			   container */
			tf->ToggleNegated();
			return child;
		} else if (auto *uf = dynamic_cast<UriSongFilter *>(child.get())) {
			/* same for #UriSongFilter */
			uf->ToggleNegated();
			return child;
		}

		nf->child = std::move(child);
	}

	return f;
}
