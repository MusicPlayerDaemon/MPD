/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "QueueCommands.hxx"
#include "CommandError.hxx"
#include "db/DatabaseQueue.hxx"
#include "SongFilter.hxx"
#include "db/Selection.hxx"
#include "Playlist.hxx"
#include "PlaylistPrint.hxx"
#include "client/ClientFile.hxx"
#include "client/Client.hxx"
#include "Partition.hxx"
#include "protocol/ArgParser.hxx"
#include "protocol/Result.hxx"
#include "ls.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "fs/AllocatedPath.hxx"

#include <limits>

#include <string.h>

CommandResult
handle_add(Client &client, gcc_unused int argc, char *argv[])
{
	char *uri = argv[1];
	PlaylistResult result;

	if (memcmp(uri, "file:///", 8) == 0) {
		const char *path_utf8 = uri + 7;
		const auto path_fs = AllocatedPath::FromUTF8(path_utf8);

		if (path_fs.IsNull()) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported file name");
			return CommandResult::ERROR;
		}

		Error error;
		if (!client_allow_file(client, path_fs, error))
			return print_error(client, error);

		result = client.partition.AppendFile(path_utf8);
		return print_playlist_result(client, result);
	}

	if (uri_has_scheme(uri)) {
		if (!uri_supported_scheme(uri)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported URI scheme");
			return CommandResult::ERROR;
		}

		result = client.partition.AppendURI(uri);
		return print_playlist_result(client, result);
	}

	const DatabaseSelection selection(uri, true);
	Error error;
	return AddFromDatabase(client.partition, selection, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_addid(Client &client, int argc, char *argv[])
{
	char *uri = argv[1];
	unsigned added_id;
	PlaylistResult result;

	if (memcmp(uri, "file:///", 8) == 0) {
		const char *path_utf8 = uri + 7;
		const auto path_fs = AllocatedPath::FromUTF8(path_utf8);

		if (path_fs.IsNull()) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported file name");
			return CommandResult::ERROR;
		}

		Error error;
		if (!client_allow_file(client, path_fs, error))
			return print_error(client, error);

		result = client.partition.AppendFile(path_utf8, &added_id);
	} else {
		if (uri_has_scheme(uri) && !uri_supported_scheme(uri)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported URI scheme");
			return CommandResult::ERROR;
		}

		result = client.partition.AppendURI(uri, &added_id);
	}

	if (result != PlaylistResult::SUCCESS)
		return print_playlist_result(client, result);

	if (argc == 3) {
		unsigned to;
		if (!check_unsigned(client, &to, argv[2]))
			return CommandResult::ERROR;
		result = client.partition.MoveId(added_id, to);
		if (result != PlaylistResult::SUCCESS) {
			CommandResult ret =
				print_playlist_result(client, result);
			client.partition.DeleteId(added_id);
			return ret;
		}
	}

	client_printf(client, "Id: %u\n", added_id);
	return CommandResult::OK;
}

CommandResult
handle_delete(Client &client, gcc_unused int argc, char *argv[])
{
	unsigned start, end;

	if (!check_range(client, &start, &end, argv[1]))
		return CommandResult::ERROR;

	PlaylistResult result = client.partition.DeleteRange(start, end);
	return print_playlist_result(client, result);
}

CommandResult
handle_deleteid(Client &client, gcc_unused int argc, char *argv[])
{
	unsigned id;

	if (!check_unsigned(client, &id, argv[1]))
		return CommandResult::ERROR;

	PlaylistResult result = client.partition.DeleteId(id);
	return print_playlist_result(client, result);
}

CommandResult
handle_playlist(Client &client,
		gcc_unused int argc, gcc_unused char *argv[])
{
	playlist_print_uris(client, client.playlist);
	return CommandResult::OK;
}

CommandResult
handle_shuffle(gcc_unused Client &client,
	       gcc_unused int argc, gcc_unused char *argv[])
{
	unsigned start = 0, end = client.playlist.queue.GetLength();
	if (argc == 2 && !check_range(client, &start, &end, argv[1]))
		return CommandResult::ERROR;

	client.partition.Shuffle(start, end);
	return CommandResult::OK;
}

CommandResult
handle_clear(gcc_unused Client &client,
	     gcc_unused int argc, gcc_unused char *argv[])
{
	client.partition.ClearQueue();
	return CommandResult::OK;
}

CommandResult
handle_plchanges(Client &client, gcc_unused int argc, char *argv[])
{
	uint32_t version;

	if (!check_uint32(client, &version, argv[1]))
		return CommandResult::ERROR;

	playlist_print_changes_info(client, client.playlist, version);
	return CommandResult::OK;
}

CommandResult
handle_plchangesposid(Client &client, gcc_unused int argc, char *argv[])
{
	uint32_t version;

	if (!check_uint32(client, &version, argv[1]))
		return CommandResult::ERROR;

	playlist_print_changes_position(client, client.playlist, version);
	return CommandResult::OK;
}

CommandResult
handle_playlistinfo(Client &client, int argc, char *argv[])
{
	unsigned start = 0, end = std::numeric_limits<unsigned>::max();
	bool ret;

	if (argc == 2 && !check_range(client, &start, &end, argv[1]))
		return CommandResult::ERROR;

	ret = playlist_print_info(client, client.playlist, start, end);
	if (!ret)
		return print_playlist_result(client,
					     PlaylistResult::BAD_RANGE);

	return CommandResult::OK;
}

CommandResult
handle_playlistid(Client &client, int argc, char *argv[])
{
	if (argc >= 2) {
		unsigned id;
		if (!check_unsigned(client, &id, argv[1]))
			return CommandResult::ERROR;

		bool ret = playlist_print_id(client, client.playlist, id);
		if (!ret)
			return print_playlist_result(client,
						     PlaylistResult::NO_SUCH_SONG);
	} else {
		playlist_print_info(client, client.playlist,
				    0, std::numeric_limits<unsigned>::max());
	}

	return CommandResult::OK;
}

static CommandResult
handle_playlist_match(Client &client, int argc, char *argv[],
		      bool fold_case)
{
	SongFilter filter;
	if (!filter.Parse(argc - 1, argv + 1, fold_case)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	playlist_print_find(client, client.playlist, filter);
	return CommandResult::OK;
}

CommandResult
handle_playlistfind(Client &client, int argc, char *argv[])
{
	return handle_playlist_match(client, argc, argv, false);
}

CommandResult
handle_playlistsearch(Client &client, int argc, char *argv[])
{
	return handle_playlist_match(client, argc, argv, true);
}

CommandResult
handle_prio(Client &client, int argc, char *argv[])
{
	unsigned priority;

	if (!check_unsigned(client, &priority, argv[1]))
		return CommandResult::ERROR;

	if (priority > 0xff) {
		command_error(client, ACK_ERROR_ARG,
			      "Priority out of range: %s", argv[1]);
		return CommandResult::ERROR;
	}

	for (int i = 2; i < argc; ++i) {
		unsigned start_position, end_position;
		if (!check_range(client, &start_position, &end_position,
				 argv[i]))
			return CommandResult::ERROR;

		PlaylistResult result =
			client.partition.SetPriorityRange(start_position,
							   end_position,
							   priority);
		if (result != PlaylistResult::SUCCESS)
			return print_playlist_result(client, result);
	}

	return CommandResult::OK;
}

CommandResult
handle_prioid(Client &client, int argc, char *argv[])
{
	unsigned priority;

	if (!check_unsigned(client, &priority, argv[1]))
		return CommandResult::ERROR;

	if (priority > 0xff) {
		command_error(client, ACK_ERROR_ARG,
			      "Priority out of range: %s", argv[1]);
		return CommandResult::ERROR;
	}

	for (int i = 2; i < argc; ++i) {
		unsigned song_id;
		if (!check_unsigned(client, &song_id, argv[i]))
			return CommandResult::ERROR;

		PlaylistResult result =
			client.partition.SetPriorityId(song_id, priority);
		if (result != PlaylistResult::SUCCESS)
			return print_playlist_result(client, result);
	}

	return CommandResult::OK;
}

CommandResult
handle_move(Client &client, gcc_unused int argc, char *argv[])
{
	unsigned start, end;
	int to;

	if (!check_range(client, &start, &end, argv[1]))
		return CommandResult::ERROR;
	if (!check_int(client, &to, argv[2]))
		return CommandResult::ERROR;

	PlaylistResult result =
		client.partition.MoveRange(start, end, to);
	return print_playlist_result(client, result);
}

CommandResult
handle_moveid(Client &client, gcc_unused int argc, char *argv[])
{
	unsigned id;
	int to;

	if (!check_unsigned(client, &id, argv[1]))
		return CommandResult::ERROR;
	if (!check_int(client, &to, argv[2]))
		return CommandResult::ERROR;
	PlaylistResult result = client.partition.MoveId(id, to);
	return print_playlist_result(client, result);
}

CommandResult
handle_swap(Client &client, gcc_unused int argc, char *argv[])
{
	unsigned song1, song2;

	if (!check_unsigned(client, &song1, argv[1]))
		return CommandResult::ERROR;
	if (!check_unsigned(client, &song2, argv[2]))
		return CommandResult::ERROR;

	PlaylistResult result =
		client.partition.SwapPositions(song1, song2);
	return print_playlist_result(client, result);
}

CommandResult
handle_swapid(Client &client, gcc_unused int argc, char *argv[])
{
	unsigned id1, id2;

	if (!check_unsigned(client, &id1, argv[1]))
		return CommandResult::ERROR;
	if (!check_unsigned(client, &id2, argv[2]))
		return CommandResult::ERROR;

	PlaylistResult result = client.partition.SwapIds(id1, id2);
	return print_playlist_result(client, result);
}
