// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_FORMAT_HXX
#define MPD_TAG_FORMAT_HXX

struct Tag;

[[gnu::malloc]] [[gnu::nonnull]]
char *
FormatTag(const Tag &tag, const char *format) noexcept;

#endif
