// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TagCommands.hxx"
#include "Request.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "tag/ParseName.hxx"
#include "tag/Type.hxx"
#include "queue/Playlist.hxx"

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
	if (args.size() >= 2) {
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
