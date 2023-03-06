// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
