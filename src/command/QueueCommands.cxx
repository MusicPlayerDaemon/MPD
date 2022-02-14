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

#include "config.h"
#include "QueueCommands.hxx"
#include "PositionArg.hxx"
#include "Request.hxx"
#include "protocol/RangeArg.hxx"
#include "db/DatabaseQueue.hxx"
#include "db/Selection.hxx"
#include "tag/ParseName.hxx"
#include "song/Filter.hxx"
#include "SongLoader.hxx"
#include "song/DetachedSong.hxx"
#include "LocateUri.hxx"
#include "queue/Playlist.hxx"
#include "queue/Selection.hxx"
#include "PlaylistPrint.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "BulkEdit.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Exception.hxx"
#include "util/StringAPI.hxx"
#include "util/NumberParser.hxx"

#include <fmt/format.h>

#include <limits>

static void
AddUri(Client &client, const LocatedUri &uri)
{
	auto &partition = client.GetPartition();
	partition.playlist.AppendSong(partition.pc,
				      SongLoader(client).LoadSong(uri));
}

#ifdef ENABLE_DATABASE

static void
AddDatabaseSelection(Partition &partition, const char *uri)
{
	const ScopeBulkEdit bulk_edit(partition);

	const DatabaseSelection selection(uri, true);
	AddFromDatabase(partition, selection);
}

#endif

CommandResult
handle_add(Client &client, Request args, [[maybe_unused]] Response &r)
{
	auto &partition = client.GetPartition();

	const char *uri = args.front();
	if (StringIsEqual(uri, "/"))
		/* this URI is malformed, but some clients are buggy
		   and use "add /" to add the whole database, which
		   was never intended to work, but once did; in order
		   to retain backwards compatibility, work around this
		   here */
		uri = "";

	const auto old_size = partition.playlist.GetLength();
	const unsigned position = args.size > 1
		? ParseInsertPosition(args[1], partition.playlist)
		: old_size;

	const auto located_uri = LocateUri(UriPluginKind::INPUT, uri,
					   &client
#ifdef ENABLE_DATABASE
					   , nullptr
#endif
					   );
	switch (located_uri.type) {
	case LocatedUri::Type::ABSOLUTE:
		AddUri(client, located_uri);
		client.GetInstance().LookupRemoteTag(located_uri.canonical_uri);
		break;

	case LocatedUri::Type::PATH:
		AddUri(client, located_uri);
		break;

	case LocatedUri::Type::RELATIVE:
#ifdef ENABLE_DATABASE
		AddDatabaseSelection(partition, located_uri.canonical_uri);
		break;
#else
		r.Error(ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
#endif
	}

	if (position < old_size) {
		const unsigned new_size = partition.playlist.GetLength();
		const RangeArg move_range{old_size, new_size};

		try {
			partition.MoveRange(move_range, position);
		} catch (...) {
			/* ignore - shall we handle it? */
		}
	}

	return CommandResult::OK;
}

CommandResult
handle_addid(Client &client, Request args, Response &r)
{
	const char *const uri = args.front();

	auto &partition = client.GetPartition();

	int to = -1;

	const auto queue_length = partition.playlist.queue.GetLength();

	if (args.size > 1)
		to = ParseInsertPosition(args[1], partition.playlist);

	const SongLoader loader(client);
	const unsigned added_position = queue_length;
	const unsigned added_id = partition.AppendURI(loader, uri);

	if (to >= 0) {
		try {
			partition.MoveRange(RangeArg::Single(added_position),
					    to);
		} catch (...) {
			/* rollback */
			partition.DeleteId(added_id);
			throw;
		}
	}

	partition.instance.LookupRemoteTag(uri);

	r.Fmt(FMT_STRING("Id: {}\n"), added_id);
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
handle_rangeid(Client &client, Request args, Response &r)
{
	unsigned id = args.ParseUnsigned(0);

	SongTime start, end;
	if (!parse_time_range(args[1], start, end)) {
		r.Error(ACK_ERROR_ARG, "Bad range");
		return CommandResult::ERROR;
	}

	client.GetPlaylist().SetSongIdRange(client.GetPlayerControl(),
					    id, start, end);
	return CommandResult::OK;
}

CommandResult
handle_delete(Client &client, Request args, [[maybe_unused]] Response &r)
{
	RangeArg range = args.ParseRange(0);
	client.GetPartition().DeleteRange(range);
	return CommandResult::OK;
}

CommandResult
handle_deleteid(Client &client, Request args, [[maybe_unused]] Response &r)
{
	unsigned id = args.ParseUnsigned(0);
	client.GetPartition().DeleteId(id);
	return CommandResult::OK;
}

CommandResult
handle_playlist(Client &client, [[maybe_unused]] Request args, Response &r)
{
	playlist_print_uris(r, client.GetPlaylist());
	return CommandResult::OK;
}

CommandResult
handle_shuffle([[maybe_unused]] Client &client, Request args, [[maybe_unused]] Response &r)
{
	RangeArg range = args.ParseOptional(0, RangeArg::All());
	client.GetPartition().Shuffle(range);
	return CommandResult::OK;
}

CommandResult
handle_clear(Client &client, [[maybe_unused]] Request args, [[maybe_unused]] Response &r)
{
	client.GetPartition().ClearQueue();
	return CommandResult::OK;
}

CommandResult
handle_plchanges(Client &client, Request args, Response &r)
{
	uint32_t version = ParseCommandArgU32(args.front());
	RangeArg range = args.ParseOptional(1, RangeArg::All());
	playlist_print_changes_info(r, client.GetPlaylist(), version, range);
	return CommandResult::OK;
}

CommandResult
handle_plchangesposid(Client &client, Request args, Response &r)
{
	uint32_t version = ParseCommandArgU32(args.front());
	RangeArg range = args.ParseOptional(1, RangeArg::All());
	playlist_print_changes_position(r, client.GetPlaylist(), version,
					range);
	return CommandResult::OK;
}

CommandResult
handle_playlistinfo(Client &client, Request args, Response &r)
{
	RangeArg range = args.ParseOptional(0, RangeArg::All());

	playlist_print_info(r, client.GetPlaylist(), range);
	return CommandResult::OK;
}

CommandResult
handle_playlistid(Client &client, Request args, Response &r)
{
	if (!args.empty()) {
		unsigned id = args.ParseUnsigned(0);
		playlist_print_id(r, client.GetPlaylist(), id);
	} else {
		playlist_print_info(r, client.GetPlaylist(), RangeArg::All());
	}

	return CommandResult::OK;
}

static TagType
ParseSortTag(const char *s)
{
	if (StringIsEqualIgnoreCase(s, "Last-Modified"))
		return TagType(SORT_TAG_LAST_MODIFIED);

	if (StringIsEqualIgnoreCase(s, "prio"))
		return TagType(SORT_TAG_PRIO);

	TagType tag = tag_name_parse_i(s);
	if (tag == TAG_NUM_OF_ITEM_TYPES)
		throw ProtocolError(ACK_ERROR_ARG, "Unknown sort tag");

	return tag;
}

static CommandResult
handle_playlist_match(Client &client, Request args, Response &r,
		      bool fold_case)
{
	RangeArg window = RangeArg::All();
	if (args.size >= 2 && StringIsEqual(args[args.size - 2], "window")) {
		window = args.ParseRange(args.size - 1);

		args.pop_back();
		args.pop_back();
	}

	TagType sort = TAG_NUM_OF_ITEM_TYPES;
	bool descending = false;
	if (args.size >= 2 && StringIsEqual(args[args.size - 2], "sort")) {
		const char *s = args.back();
		if (*s == '-') {
			descending = true;
			++s;
		}

		sort = ParseSortTag(s);

		args.pop_back();
		args.pop_back();
	}

	SongFilter filter;
	try {
		filter.Parse(args, fold_case);
	} catch (...) {
		r.Error(ACK_ERROR_ARG,
			GetFullMessage(std::current_exception()).c_str());
		return CommandResult::ERROR;
	}
	filter.Optimize();

	QueueSelection selection;
	selection.filter = &filter;
	selection.window = window;
	selection.sort = sort;
	selection.descending = descending;

	playlist_print_find(r, client.GetPlaylist(), selection);
	return CommandResult::OK;
}

CommandResult
handle_playlistfind(Client &client, Request args, Response &r)
{
	return handle_playlist_match(client, args, r, false);
}

CommandResult
handle_playlistsearch(Client &client, Request args, Response &r)
{
	return handle_playlist_match(client, args, r, true);
}

CommandResult
handle_prio(Client &client, Request args, [[maybe_unused]] Response &r)
{
	unsigned priority = args.ParseUnsigned(0, 0xff);
	args.shift();

	auto &partition = client.GetPartition();

	for (const char *i : args) {
		RangeArg range = ParseCommandArgRange(i);
		partition.SetPriorityRange(range, priority);
	}

	return CommandResult::OK;
}

CommandResult
handle_prioid(Client &client, Request args, [[maybe_unused]] Response &r)
{
	unsigned priority = args.ParseUnsigned(0, 0xff);
	args.shift();

	auto &partition = client.GetPartition();

	for (const char *i : args) {
		unsigned song_id = ParseCommandArgUnsigned(i);
		partition.SetPriorityId(song_id, priority);
	}

	return CommandResult::OK;
}

static CommandResult
handle_move(Partition &partition, RangeArg range, const char *to)
{
	partition.MoveRange(range,
			    ParseMoveDestination(to, range,
						 partition.playlist));
	return CommandResult::OK;
}

CommandResult
handle_move(Client &client, Request args, Response &r)
{
	RangeArg range = args.ParseRange(0);
	if (range.IsOpenEnded()) {
		r.Error(ACK_ERROR_ARG, "Open-ended range not supported");
		return CommandResult::ERROR;
	}

	return handle_move(client.GetPartition(), range, args[1]);
}

CommandResult
handle_moveid(Client &client, Request args, [[maybe_unused]] Response &r)
{
	auto &partition = client.GetPartition();
	const auto &queue = partition.playlist.queue;

	const int position = queue.IdToPosition(args.ParseUnsigned(0));
	if (position < 0) {
		r.Error(ACK_ERROR_NO_EXIST, "No such song");
		return CommandResult::ERROR;
	}

	return handle_move(partition, RangeArg::Single(position), args[1]);
}

CommandResult
handle_swap(Client &client, Request args, [[maybe_unused]] Response &r)
{
	unsigned song1 = args.ParseUnsigned(0);
	unsigned song2 = args.ParseUnsigned(1);
	client.GetPartition().SwapPositions(song1, song2);
	return CommandResult::OK;
}

CommandResult
handle_swapid(Client &client, Request args, [[maybe_unused]] Response &r)
{
	unsigned id1 = args.ParseUnsigned(0);
	unsigned id2 = args.ParseUnsigned(1);
	client.GetPartition().SwapIds(id1, id2);
	return CommandResult::OK;
}
