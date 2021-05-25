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

#include "TagCommands.hxx"
#include "Request.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "tag/ParseName.hxx"
#include "queue/Playlist.hxx"
#include "util/ConstBuffer.hxx"

#include <fmt/format.h>

CommandResult
handle_addtagid(Client &client, Request args, Response &r)
{
	unsigned song_id = args.ParseUnsigned(0);

	const char *const tag_name = args[1];
	const TagType tag_type = tag_name_parse_i(tag_name);
	if (tag_type == TAG_NUM_OF_ITEM_TYPES) {
		r.FmtError(ACK_ERROR_ARG, FMT_STRING("Unknown tag type: {}"),
			   tag_name);
		return CommandResult::ERROR;
	}

	const char *const value = args[2];

	client.GetPlaylist().AddSongIdTag(song_id, tag_type, value);
	return CommandResult::OK;
}

CommandResult
handle_cleartagid(Client &client, Request args, Response &r)
{
	unsigned song_id = args.ParseUnsigned(0);

	TagType tag_type = TAG_NUM_OF_ITEM_TYPES;
	if (args.size >= 2) {
		const char *const tag_name = args[1];
		tag_type = tag_name_parse_i(tag_name);
		if (tag_type == TAG_NUM_OF_ITEM_TYPES) {
			r.FmtError(ACK_ERROR_ARG,
				   FMT_STRING("Unknown tag type: {}"),
				   tag_name);
			return CommandResult::ERROR;
		}
	}

	client.GetPlaylist().ClearSongIdTag(song_id, tag_type);
	return CommandResult::OK;
}
