/*
 * Copyright 2003-2019 The Music Player Daemon Project
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
#include "PlaylistCommands.hxx"
#include "Request.hxx"
#include "Instance.hxx"
#include "db/Selection.hxx"
#include "db/DatabasePlaylist.hxx"
#include "PlaylistSave.hxx"
#include "PlaylistFile.hxx"
#include "PlaylistError.hxx"
#include "db/PlaylistVector.hxx"
#include "SongLoader.hxx"
#include "song/DetachedSong.hxx"
#include "BulkEdit.hxx"
#include "playlist/PlaylistQueue.hxx"
#include "playlist/Print.hxx"
#include "TimePrint.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Mapper.hxx"
#include "fs/AllocatedPath.hxx"
#include "time/ChronoUtil.hxx"
#include "util/ConstBuffer.hxx"
#include "util/UriExtract.hxx"
#include "LocateUri.hxx"

bool
playlist_commands_available() noexcept
{
	return !map_spl_path().IsNull();
}

static void
print_spl_list(Response &r, const PlaylistVector &list)
{
	for (const auto &i : list) {
		r.Format("playlist: %s\n", i.name.c_str());

		if (!IsNegative(i.mtime))
			time_print(r, "Last-Modified", i.mtime);
	}
}

CommandResult
handle_save(Client &client, Request args, gcc_unused Response &r)
{
	spl_save_playlist(args.front(), client.GetPlaylist());
	return CommandResult::OK;
}

CommandResult
handle_load(Client &client, Request args, gcc_unused Response &r)
{
	const auto uri = LocateUri(UriPluginKind::PLAYLIST, args.front(),
				   &client
#ifdef ENABLE_DATABASE
					   , nullptr
#endif
					   );
	RangeArg range = args.ParseOptional(1, RangeArg::All());

	const ScopeBulkEdit bulk_edit(client.GetPartition());

	auto &playlist = client.GetPlaylist();
	const unsigned old_size = playlist.GetLength();

	const SongLoader loader(client);
	playlist_open_into_queue(uri,
				 range.start, range.end,
				 playlist,
				 client.GetPlayerControl(), loader);

	/* invoke the RemoteTagScanner on all newly added songs */
	auto &instance = client.GetInstance();
	const unsigned new_size = playlist.GetLength();
	for (unsigned i = old_size; i < new_size; ++i)
		instance.LookupRemoteTag(playlist.queue.Get(i).GetURI());

	return CommandResult::OK;
}

CommandResult
handle_listplaylist(Client &client, Request args, Response &r)
{
	const auto name = LocateUri(UriPluginKind::PLAYLIST, args.front(),
				    &client
#ifdef ENABLE_DATABASE
					   , nullptr
#endif
					   );

	if (playlist_file_print(r, client.GetPartition(), SongLoader(client),
				name, false))
		return CommandResult::OK;

	throw PlaylistError::NoSuchList();
}

CommandResult
handle_listplaylistinfo(Client &client, Request args, Response &r)
{
	const auto name = LocateUri(UriPluginKind::PLAYLIST, args.front(),
				    &client
#ifdef ENABLE_DATABASE
					   , nullptr
#endif
					   );

	if (playlist_file_print(r, client.GetPartition(), SongLoader(client),
				name, true))
		return CommandResult::OK;

	throw PlaylistError::NoSuchList();
}

CommandResult
handle_rm(gcc_unused Client &client, Request args, gcc_unused Response &r)
{
	const char *const name = args.front();

	spl_delete(name);
	return CommandResult::OK;
}

CommandResult
handle_rename(gcc_unused Client &client, Request args, gcc_unused Response &r)
{
	const char *const old_name = args[0];
	const char *const new_name = args[1];

	spl_rename(old_name, new_name);
	return CommandResult::OK;
}

CommandResult
handle_playlistdelete(gcc_unused Client &client,
		      Request args, gcc_unused Response &r)
{
	const char *const name = args[0];
	unsigned from = args.ParseUnsigned(1);

	spl_remove_index(name, from);
	return CommandResult::OK;
}

CommandResult
handle_playlistmove(gcc_unused Client &client,
		    Request args, gcc_unused Response &r)
{
	const char *const name = args.front();
	unsigned from = args.ParseUnsigned(1);
	unsigned to = args.ParseUnsigned(2);

	spl_move_index(name, from, to);
	return CommandResult::OK;
}

CommandResult
handle_playlistclear(gcc_unused Client &client,
		     Request args, gcc_unused Response &r)
{
	const char *const name = args.front();

	spl_clear(name);
	return CommandResult::OK;
}

CommandResult
handle_playlistadd(Client &client, Request args, gcc_unused Response &r)
{
	const char *const playlist = args[0];
	const char *const uri = args[1];

	if (uri_has_scheme(uri)) {
		const SongLoader loader(client);
		spl_append_uri(playlist, loader, uri);
	} else {
#ifdef ENABLE_DATABASE
		const Database &db = client.GetDatabaseOrThrow();
		const DatabaseSelection selection(uri, true, nullptr);

		search_add_to_playlist(db, client.GetStorage(),
				       playlist, selection);
#else
		r.Error(ACK_ERROR_NO_EXIST, "directory or file not found");
		return CommandResult::ERROR;
#endif
	}

	return CommandResult::OK;
}

CommandResult
handle_listplaylists(gcc_unused Client &client, gcc_unused Request args,
		     Response &r)
{
	print_spl_list(r, ListPlaylistFiles());
	return CommandResult::OK;
}
