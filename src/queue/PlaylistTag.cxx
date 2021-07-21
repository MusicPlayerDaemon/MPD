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
