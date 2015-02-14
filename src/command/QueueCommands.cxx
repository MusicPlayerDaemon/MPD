/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "db/Selection.hxx"
#include "SongFilter.hxx"
#include "SongLoader.hxx"
#include "queue/Playlist.hxx"
#include "PlaylistPrint.hxx"
#include "client/Client.hxx"
#include "Partition.hxx"
#include "BulkEdit.hxx"
#include "protocol/ArgParser.hxx"
#include "protocol/Result.hxx"
#include "ls.hxx"
#include "util/ConstBuffer.hxx"
#include "util/UriUtil.hxx"
#include "util/NumberParser.hxx"
#include "util/Error.hxx"
#include "fs/AllocatedPath.hxx"

#include <limits>

#include <string.h>

static const char *
translate_uri(const char *uri)
{
	if (memcmp(uri, "file:///", 8) == 0)
		/* drop the "file://", leave only an absolute path
		   (starting with a slash) */
		return uri + 7;

	return uri;
}

CommandResult
handle_add(Client &client, ConstBuffer<const char *> args)
{
	const char *uri = args.front();
	if (memcmp(uri, "/", 2) == 0)
		/* this URI is malformed, but some clients are buggy
		   and use "add /" to add the whole database, which
		   was never intended to work, but once did; in order
		   to retain backwards compatibility, work around this
		   here */
		uri = "";

	uri = translate_uri(uri);

	if (uri_has_scheme(uri) || PathTraitsUTF8::IsAbsolute(uri)) {
		const SongLoader loader(client);
		Error error;
		unsigned id = client.partition.AppendURI(loader, uri, error);
		if (id == 0)
			return print_error(client, error);

		return CommandResult::OK;
	}

#ifdef ENABLE_DATABASE
	const ScopeBulkEdit bulk_edit(client.partition);

	const DatabaseSelection selection(uri, true);
	Error error;
	return AddFromDatabase(client.partition, selection, error)
		? CommandResult::OK
		: print_error(client, error);
#else
	command_error(client, ACK_ERROR_NO_EXIST, "No database");
	return CommandResult::ERROR;
#endif
}

CommandResult
handle_addid(Client &client, ConstBuffer<const char *> args)
{
	const char *const uri = translate_uri(args.front());

	const SongLoader loader(client);
	Error error;
	unsigned added_id = client.partition.AppendURI(loader, uri, error);
	if (added_id == 0)
		return print_error(client, error);

	if (args.size == 2) {
		unsigned to;
		if (!check_unsigned(client, &to, args[1]))
			return CommandResult::ERROR;
		PlaylistResult result = client.partition.MoveId(added_id, to);
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

/**
 * Parse a string in the form "START:END", both being (optional)
 * fractional non-negative time offsets in seconds.  Returns both in
 * integer milliseconds.  Omitted values are zero.
 */
static bool
parse_time_range(const char *p, SongTime &start_r, SongTime &end_r)
{
	char *endptr;

	const float start = ParseFloat(p, &endptr);
	if (*endptr != ':' || start < 0)
		return false;

	start_r = endptr > p
		? SongTime::FromS(start)
		: SongTime::zero();

	p = endptr + 1;

	const float end = ParseFloat(p, &endptr);
	if (*endptr != 0 || end < 0)
		return false;

	end_r = endptr > p
		? SongTime::FromS(end)
		: SongTime::zero();

	return end_r.IsZero() || end_r > start_r;
}

CommandResult
handle_rangeid(Client &client, ConstBuffer<const char *> args)
{
	unsigned id;
	if (!check_unsigned(client, &id, args.front()))
		return CommandResult::ERROR;

	SongTime start, end;
	if (!parse_time_range(args[1], start, end)) {
		command_error(client, ACK_ERROR_ARG, "Bad range");
		return CommandResult::ERROR;
	}

	Error error;
	if (!client.partition.playlist.SetSongIdRange(client.partition.pc,
						      id, start, end,
						      error))
		return print_error(client, error);

	return CommandResult::OK;
}

CommandResult
handle_delete(Client &client, ConstBuffer<const char *> args)
{
	unsigned start, end;

	if (!check_range(client, &start, &end, args.front()))
		return CommandResult::ERROR;

	PlaylistResult result = client.partition.DeleteRange(start, end);
	return print_playlist_result(client, result);
}

CommandResult
handle_deleteid(Client &client, ConstBuffer<const char *> args)
{
	unsigned id;

	if (!check_unsigned(client, &id, args.front()))
		return CommandResult::ERROR;

	PlaylistResult result = client.partition.DeleteId(id);
	return print_playlist_result(client, result);
}

CommandResult
handle_playlist(Client &client, gcc_unused ConstBuffer<const char *> args)
{
	playlist_print_uris(client, client.playlist);
	return CommandResult::OK;
}

CommandResult
handle_shuffle(gcc_unused Client &client, ConstBuffer<const char *> args)
{
	unsigned start = 0, end = client.playlist.queue.GetLength();
	if (args.size == 1 && !check_range(client, &start, &end, args.front()))
		return CommandResult::ERROR;

	client.partition.Shuffle(start, end);
	return CommandResult::OK;
}

CommandResult
handle_clear(gcc_unused Client &client, gcc_unused ConstBuffer<const char *> args)
{
	client.partition.ClearQueue();
	return CommandResult::OK;
}

CommandResult
handle_plchanges(Client &client, ConstBuffer<const char *> args)
{
	uint32_t version;

	if (!check_uint32(client, &version, args.front()))
		return CommandResult::ERROR;

	playlist_print_changes_info(client, client.playlist, version);
	return CommandResult::OK;
}

CommandResult
handle_plchangesposid(Client &client, ConstBuffer<const char *> args)
{
	uint32_t version;

	if (!check_uint32(client, &version, args.front()))
		return CommandResult::ERROR;

	playlist_print_changes_position(client, client.playlist, version);
	return CommandResult::OK;
}

CommandResult
handle_playlistinfo(Client &client, ConstBuffer<const char *> args)
{
	unsigned start = 0, end = std::numeric_limits<unsigned>::max();
	bool ret;

	if (args.size == 1 && !check_range(client, &start, &end, args.front()))
		return CommandResult::ERROR;

	ret = playlist_print_info(client, client.playlist, start, end);
	if (!ret)
		return print_playlist_result(client,
					     PlaylistResult::BAD_RANGE);

	return CommandResult::OK;
}

CommandResult
handle_playlistid(Client &client, ConstBuffer<const char *> args)
{
	if (!args.IsEmpty()) {
		unsigned id;
		if (!check_unsigned(client, &id, args.front()))
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
handle_playlist_match(Client &client, ConstBuffer<const char *> args,
		      bool fold_case)
{
	SongFilter filter;
	if (!filter.Parse(args, fold_case)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	playlist_print_find(client, client.playlist, filter);
	return CommandResult::OK;
}

CommandResult
handle_playlistfind(Client &client, ConstBuffer<const char *> args)
{
	return handle_playlist_match(client, args, false);
}

CommandResult
handle_playlistsearch(Client &client, ConstBuffer<const char *> args)
{
	return handle_playlist_match(client, args, true);
}

CommandResult
handle_prio(Client &client, ConstBuffer<const char *> args)
{
	const char *const priority_string = args.shift();
	unsigned priority;

	if (!check_unsigned(client, &priority, priority_string))
		return CommandResult::ERROR;

	if (priority > 0xff) {
		command_error(client, ACK_ERROR_ARG,
			      "Priority out of range: %s", priority_string);
		return CommandResult::ERROR;
	}

	for (const char *i : args) {
		unsigned start_position, end_position;
		if (!check_range(client, &start_position, &end_position, i))
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
handle_prioid(Client &client, ConstBuffer<const char *> args)
{
	const char *const priority_string = args.shift();
	unsigned priority;

	if (!check_unsigned(client, &priority, priority_string))
		return CommandResult::ERROR;

	if (priority > 0xff) {
		command_error(client, ACK_ERROR_ARG,
			      "Priority out of range: %s", priority_string);
		return CommandResult::ERROR;
	}

	for (const char *i : args) {
		unsigned song_id;
		if (!check_unsigned(client, &song_id, i))
			return CommandResult::ERROR;

		PlaylistResult result =
			client.partition.SetPriorityId(song_id, priority);
		if (result != PlaylistResult::SUCCESS)
			return print_playlist_result(client, result);
	}

	return CommandResult::OK;
}

CommandResult
handle_move(Client &client, ConstBuffer<const char *> args)
{
	unsigned start, end;
	int to;

	if (!check_range(client, &start, &end, args[0]))
		return CommandResult::ERROR;
	if (!check_int(client, &to, args[1]))
		return CommandResult::ERROR;

	PlaylistResult result =
		client.partition.MoveRange(start, end, to);
	return print_playlist_result(client, result);
}

CommandResult
handle_moveid(Client &client, ConstBuffer<const char *> args)
{
	unsigned id;
	int to;

	if (!check_unsigned(client, &id, args[0]))
		return CommandResult::ERROR;
	if (!check_int(client, &to, args[1]))
		return CommandResult::ERROR;
	PlaylistResult result = client.partition.MoveId(id, to);
	return print_playlist_result(client, result);
}

CommandResult
handle_swap(Client &client, ConstBuffer<const char *> args)
{
	unsigned song1, song2;

	if (!check_unsigned(client, &song1, args[0]))
		return CommandResult::ERROR;
	if (!check_unsigned(client, &song2, args[1]))
		return CommandResult::ERROR;

	PlaylistResult result =
		client.partition.SwapPositions(song1, song2);
	return print_playlist_result(client, result);
}

CommandResult
handle_swapid(Client &client, ConstBuffer<const char *> args)
{
	unsigned id1, id2;

	if (!check_unsigned(client, &id1, args[0]))
		return CommandResult::ERROR;
	if (!check_unsigned(client, &id2, args[1]))
		return CommandResult::ERROR;

	PlaylistResult result = client.partition.SwapIds(id1, id2);
	return print_playlist_result(client, result);
}
