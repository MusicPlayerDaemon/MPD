// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TagSave.hxx"
#include "tag/Names.hxx"
#include "tag/Tag.hxx"
#include "io/BufferedOutputStream.hxx"

#include <fmt/format.h>

#define SONG_TIME "Time: "

void
tag_save(BufferedOutputStream &os, const Tag &tag)
{
	if (!tag.duration.IsNegative())
		os.Fmt(FMT_STRING(SONG_TIME "{}\n"), tag.duration.ToDoubleS());

	if (tag.has_playlist)
		os.Write("Playlist: yes\n");

	for (const auto &i : tag)
		os.Fmt(FMT_STRING("{}: {}\n"),
		       tag_item_names[i.type], i.value);
}
