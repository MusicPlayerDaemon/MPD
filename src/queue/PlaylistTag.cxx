// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Functions for editing the playlist (adding, removing, reordering
 * songs in the queue).
 *
 */

#include "Playlist.hxx"
#include "PlaylistError.hxx"
#include "song/DetachedSong.hxx"
#include "tag/Tag.hxx"
#include "tag/Builder.hxx"

void
playlist::AddSongIdTag(unsigned id, TagType tag_type, const char *value)
{
	const int position = queue.IdToPosition(id);
	if (position < 0)
		throw PlaylistError::NoSuchSong();

	DetachedSong &song = queue.Get(position);
	if (song.IsFile())
		throw PlaylistError(PlaylistResult::DENIED,
				    "Cannot edit tags of local file");

	{
		TagBuilder tag(std::move(song.WritableTag()));
		tag.AddItem(tag_type, value);
		song.SetTag(tag.Commit());
	}

	queue.ModifyAtPosition(position);
	OnModified();
}

void
playlist::ClearSongIdTag(unsigned id, TagType tag_type)
{
	const int position = queue.IdToPosition(id);
	if (position < 0)
		throw PlaylistError::NoSuchSong();

	DetachedSong &song = queue.Get(position);
	if (song.IsFile())
		throw PlaylistError(PlaylistResult::DENIED,
				    "Cannot edit tags of local file");

	{
		TagBuilder tag(std::move(song.WritableTag()));
		if (tag_type == TAG_NUM_OF_ITEM_TYPES)
			tag.RemoveAll();
		else
			tag.RemoveType(tag_type);
		song.SetTag(tag.Commit());
	}

	queue.ModifyAtPosition(position);
	OnModified();
}
