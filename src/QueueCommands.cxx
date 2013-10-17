/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "DatabaseQueue.hxx"
#include "SongFilter.hxx"
#include "DatabaseSelection.hxx"
#include "Playlist.hxx"
#include "PlaylistPrint.hxx"
#include "ClientFile.hxx"
#include "ClientInternal.hxx"
#include "Partition.hxx"
#include "protocol/ArgParser.hxx"
#include "protocol/Result.hxx"
#include "ls.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "fs/AllocatedPath.hxx"

#include <limits>

#include <string.h>

enum command_return
handle_add(Client *client, gcc_unused int argc, char *argv[])
{
	char *uri = argv[1];
	enum playlist_result result;

	if (memcmp(uri, "file:///", 8) == 0) {
		const char *path_utf8 = uri + 7;
		const auto path_fs = AllocatedPath::FromUTF8(path_utf8);

		if (path_fs.IsNull()) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported file name");
			return COMMAND_RETURN_ERROR;
		}

		Error error;
		if (!client_allow_file(client, path_fs, error))
			return print_error(client, error);

		result = client->partition.AppendFile(path_utf8);
		return print_playlist_result(client, result);
	}

	if (uri_has_scheme(uri)) {
		if (!uri_supported_scheme(uri)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported URI scheme");
			return COMMAND_RETURN_ERROR;
		}

		result = client->partition.AppendURI(uri);
		return print_playlist_result(client, result);
	}

	const DatabaseSelection selection(uri, true);
	Error error;
	return AddFromDatabase(client->partition, selection, error)
		? COMMAND_RETURN_OK
		: print_error(client, error);
}

enum command_return
handle_addid(Client *client, int argc, char *argv[])
{
	char *uri = argv[1];
	unsigned added_id;
	enum playlist_result result;

	if (memcmp(uri, "file:///", 8) == 0) {
		const char *path_utf8 = uri + 7;
		const auto path_fs = AllocatedPath::FromUTF8(path_utf8);

		if (path_fs.IsNull()) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported file name");
			return COMMAND_RETURN_ERROR;
		}

		Error error;
		if (!client_allow_file(client, path_fs, error))
			return print_error(client, error);

		result = client->partition.AppendFile(path_utf8, &added_id);
	} else {
		if (uri_has_scheme(uri) && !uri_supported_scheme(uri)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported URI scheme");
			return COMMAND_RETURN_ERROR;
		}

		result = client->partition.AppendURI(uri, &added_id);
	}

	if (result != PLAYLIST_RESULT_SUCCESS)
		return print_playlist_result(client, result);

	if (argc == 3) {
		unsigned to;
		if (!check_unsigned(client, &to, argv[2]))
			return COMMAND_RETURN_ERROR;
		result = client->partition.MoveId(added_id, to);
		if (result != PLAYLIST_RESULT_SUCCESS) {
			enum command_return ret =
				print_playlist_result(client, result);
			client->partition.DeleteId(added_id);
			return ret;
		}
	}

	client_printf(client, "Id: %u\n", added_id);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_delete(Client *client, gcc_unused int argc, char *argv[])
{
	unsigned start, end;

	if (!check_range(client, &start, &end, argv[1]))
		return COMMAND_RETURN_ERROR;

	enum playlist_result result = client->partition.DeleteRange(start, end);
	return print_playlist_result(client, result);
}

enum command_return
handle_deleteid(Client *client, gcc_unused int argc, char *argv[])
{
	unsigned id;

	if (!check_unsigned(client, &id, argv[1]))
		return COMMAND_RETURN_ERROR;

	enum playlist_result result = client->partition.DeleteId(id);
	return print_playlist_result(client, result);
}

enum command_return
handle_playlist(Client *client,
		gcc_unused int argc, gcc_unused char *argv[])
{
	playlist_print_uris(client, &client->playlist);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_shuffle(gcc_unused Client *client,
	       gcc_unused int argc, gcc_unused char *argv[])
{
	unsigned start = 0, end = client->playlist.queue.GetLength();
	if (argc == 2 && !check_range(client, &start, &end, argv[1]))
		return COMMAND_RETURN_ERROR;

	client->partition.Shuffle(start, end);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_clear(gcc_unused Client *client,
	     gcc_unused int argc, gcc_unused char *argv[])
{
	client->partition.ClearQueue();
	return COMMAND_RETURN_OK;
}

enum command_return
handle_plchanges(Client *client, gcc_unused int argc, char *argv[])
{
	uint32_t version;

	if (!check_uint32(client, &version, argv[1]))
		return COMMAND_RETURN_ERROR;

	playlist_print_changes_info(client, &client->playlist, version);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_plchangesposid(Client *client, gcc_unused int argc, char *argv[])
{
	uint32_t version;

	if (!check_uint32(client, &version, argv[1]))
		return COMMAND_RETURN_ERROR;

	playlist_print_changes_position(client, &client->playlist, version);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_playlistinfo(Client *client, int argc, char *argv[])
{
	unsigned start = 0, end = std::numeric_limits<unsigned>::max();
	bool ret;

	if (argc == 2 && !check_range(client, &start, &end, argv[1]))
		return COMMAND_RETURN_ERROR;

	ret = playlist_print_info(client, &client->playlist, start, end);
	if (!ret)
		return print_playlist_result(client,
					     PLAYLIST_RESULT_BAD_RANGE);

	return COMMAND_RETURN_OK;
}

enum command_return
handle_playlistid(Client *client, int argc, char *argv[])
{
	if (argc >= 2) {
		unsigned id;
		if (!check_unsigned(client, &id, argv[1]))
			return COMMAND_RETURN_ERROR;

		bool ret = playlist_print_id(client, &client->playlist, id);
		if (!ret)
			return print_playlist_result(client,
						     PLAYLIST_RESULT_NO_SUCH_SONG);
	} else {
		playlist_print_info(client, &client->playlist,
				    0, std::numeric_limits<unsigned>::max());
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_playlist_match(Client *client, int argc, char *argv[],
		      bool fold_case)
{
	SongFilter filter;
	if (!filter.Parse(argc - 1, argv + 1, fold_case)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	playlist_print_find(client, &client->playlist, filter);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_playlistfind(Client *client, int argc, char *argv[])
{
	return handle_playlist_match(client, argc, argv, false);
}

enum command_return
handle_playlistsearch(Client *client, int argc, char *argv[])
{
	return handle_playlist_match(client, argc, argv, true);
}

enum command_return
handle_prio(Client *client, int argc, char *argv[])
{
	unsigned priority;

	if (!check_unsigned(client, &priority, argv[1]))
		return COMMAND_RETURN_ERROR;

	if (priority > 0xff) {
		command_error(client, ACK_ERROR_ARG,
			      "Priority out of range: %s", argv[1]);
		return COMMAND_RETURN_ERROR;
	}

	for (int i = 2; i < argc; ++i) {
		unsigned start_position, end_position;
		if (!check_range(client, &start_position, &end_position,
				 argv[i]))
			return COMMAND_RETURN_ERROR;

		enum playlist_result result =
			client->partition.SetPriorityRange(start_position,
							   end_position,
							   priority);
		if (result != PLAYLIST_RESULT_SUCCESS)
			return print_playlist_result(client, result);
	}

	return COMMAND_RETURN_OK;
}

enum command_return
handle_prioid(Client *client, int argc, char *argv[])
{
	unsigned priority;

	if (!check_unsigned(client, &priority, argv[1]))
		return COMMAND_RETURN_ERROR;

	if (priority > 0xff) {
		command_error(client, ACK_ERROR_ARG,
			      "Priority out of range: %s", argv[1]);
		return COMMAND_RETURN_ERROR;
	}

	for (int i = 2; i < argc; ++i) {
		unsigned song_id;
		if (!check_unsigned(client, &song_id, argv[i]))
			return COMMAND_RETURN_ERROR;

		enum playlist_result result =
			client->partition.SetPriorityId(song_id, priority);
		if (result != PLAYLIST_RESULT_SUCCESS)
			return print_playlist_result(client, result);
	}

	return COMMAND_RETURN_OK;
}

enum command_return
handle_move(Client *client, gcc_unused int argc, char *argv[])
{
	unsigned start, end;
	int to;

	if (!check_range(client, &start, &end, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &to, argv[2]))
		return COMMAND_RETURN_ERROR;

	enum playlist_result result =
		client->partition.MoveRange(start, end, to);
	return print_playlist_result(client, result);
}

enum command_return
handle_moveid(Client *client, gcc_unused int argc, char *argv[])
{
	unsigned id;
	int to;

	if (!check_unsigned(client, &id, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &to, argv[2]))
		return COMMAND_RETURN_ERROR;
	enum playlist_result result = client->partition.MoveId(id, to);
	return print_playlist_result(client, result);
}

enum command_return
handle_swap(Client *client, gcc_unused int argc, char *argv[])
{
	unsigned song1, song2;

	if (!check_unsigned(client, &song1, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_unsigned(client, &song2, argv[2]))
		return COMMAND_RETURN_ERROR;

	enum playlist_result result =
		client->partition.SwapPositions(song1, song2);
	return print_playlist_result(client, result);
}

enum command_return
handle_swapid(Client *client, gcc_unused int argc, char *argv[])
{
	unsigned id1, id2;

	if (!check_unsigned(client, &id1, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_unsigned(client, &id2, argv[2]))
		return COMMAND_RETURN_ERROR;

	enum playlist_result result = client->partition.SwapIds(id1, id2);
	return print_playlist_result(client, result);
}
