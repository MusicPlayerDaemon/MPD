// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_SETTINGS_HXX
#define MPD_TAG_SETTINGS_HXX

#include "Mask.hxx"

extern TagMask global_tag_mask;

[[gnu::const]]
static inline bool
IsTagEnabled(TagType tag) noexcept
{
	return global_tag_mask.Test(tag);
}

[[gnu::const]]
static inline bool
IsTagEnabled(unsigned tag) noexcept
{
	return IsTagEnabled(TagType(tag));
}

#endif
