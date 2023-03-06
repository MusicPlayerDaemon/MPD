// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
