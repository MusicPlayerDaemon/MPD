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
#include "Playlist.hxx"
#include "PlaylistPrint.hxx"
#include "ClientFile.hxx"
#include "ClientInternal.hxx"
#include "protocol/ArgParser.hxx"
#include "protocol/Result.hxx"
#include "ls.hxx"

extern "C" {
#include "uri.h"
}

#include <string.h>

enum command_return
handle_add(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	char *uri = argv[1];
	enum playlist_result result;

	if (strncmp(uri, "file:///", 8) == 0) {
		const char *path = uri + 7;

		GError *error = NULL;
		if (!client_allow_file(client, path, &error))
			return print_error(client, error);

		result = playlist_append_file(&g_playlist,
					      client->player_control,
					      path,
					      NULL);
		return print_playlist_result(client, result);
	}

	if (uri_has_scheme(uri)) {
		if (!uri_supported_scheme(uri)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported URI scheme");
			return COMMAND_RETURN_ERROR;
		}

		result = playlist_append_uri(&g_playlist,
					     client->player_control,
					     uri, NULL);
		return print_playlist_result(client, result);
	}

	GError *error = NULL;
	return findAddIn(client->player_control, uri, nullptr, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);
}

enum command_return
handle_addid(Client *client, int argc, char *argv[])
{
	char *uri = argv[1];
	unsigned added_id;
	enum playlist_result result;

	if (strncmp(uri, "file:///", 8) == 0) {
		const char *path = uri + 7;

		GError *error = NULL;
		if (!client_allow_file(client, path, &error))
			return print_error(client, error);

		result = playlist_append_file(&g_playlist,
					      client->player_control,
					      path,
					      &added_id);
	} else {
		if (uri_has_scheme(uri) && !uri_supported_scheme(uri)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported URI scheme");
			return COMMAND_RETURN_ERROR;
		}

		result = playlist_append_uri(&g_playlist,
					     client->player_control,
					     uri, &added_id);
	}

	if (result != PLAYLIST_RESULT_SUCCESS)
		return print_playlist_result(client, result);

	if (argc == 3) {
		unsigned to;
		if (!check_unsigned(client, &to, argv[2]))
			return COMMAND_RETURN_ERROR;
		result = playlist_move_id(&g_playlist, client->player_control,
					  added_id, to);
		if (result != PLAYLIST_RESULT_SUCCESS) {
			enum command_return ret =
				print_playlist_result(client, result);
			playlist_delete_id(&g_playlist, client->player_control,
					   added_id);
			return ret;
		}
	}

	client_printf(client, "Id: %u\n", added_id);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_delete(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned start, end;
	enum playlist_result result;

	if (!check_range(client, &start, &end, argv[1]))
		return COMMAND_RETURN_ERROR;

	result = playlist_delete_range(&g_playlist, client->player_control,
				       start, end);
	return print_playlist_result(client, result);
}

enum command_return
handle_deleteid(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned id;
	enum playlist_result result;

	if (!check_unsigned(client, &id, argv[1]))
		return COMMAND_RETURN_ERROR;

	result = playlist_delete_id(&g_playlist, client->player_control, id);
	return print_playlist_result(client, result);
}

enum command_return
handle_playlist(Client *client,
	        G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	playlist_print_uris(client, &g_playlist);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_shuffle(G_GNUC_UNUSED Client *client,
	       G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	unsigned start = 0, end = queue_length(&g_playlist.queue);
	if (argc == 2 && !check_range(client, &start, &end, argv[1]))
		return COMMAND_RETURN_ERROR;

	playlist_shuffle(&g_playlist, client->player_control, start, end);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_clear(G_GNUC_UNUSED Client *client,
	     G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	playlist_clear(&g_playlist, client->player_control);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_plchanges(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	uint32_t version;

	if (!check_uint32(client, &version, argv[1]))
		return COMMAND_RETURN_ERROR;

	playlist_print_changes_info(client, &g_playlist, version);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_plchangesposid(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	uint32_t version;

	if (!check_uint32(client, &version, argv[1]))
		return COMMAND_RETURN_ERROR;

	playlist_print_changes_position(client, &g_playlist, version);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_playlistinfo(Client *client, int argc, char *argv[])
{
	unsigned start = 0, end = G_MAXUINT;
	bool ret;

	if (argc == 2 && !check_range(client, &start, &end, argv[1]))
		return COMMAND_RETURN_ERROR;

	ret = playlist_print_info(client, &g_playlist, start, end);
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

		bool ret = playlist_print_id(client, &g_playlist, id);
		if (!ret)
			return print_playlist_result(client,
						     PLAYLIST_RESULT_NO_SUCH_SONG);
	} else {
		playlist_print_info(client, &g_playlist, 0, G_MAXUINT);
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

	playlist_print_find(client, &g_playlist, filter);
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
			playlist_set_priority(&g_playlist,
					      client->player_control,
					      start_position, end_position,
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
			playlist_set_priority_id(&g_playlist,
						 client->player_control,
						 song_id, priority);
		if (result != PLAYLIST_RESULT_SUCCESS)
			return print_playlist_result(client, result);
	}

	return COMMAND_RETURN_OK;
}

enum command_return
handle_move(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned start, end;
	int to;
	enum playlist_result result;

	if (!check_range(client, &start, &end, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &to, argv[2]))
		return COMMAND_RETURN_ERROR;
	result = playlist_move_range(&g_playlist, client->player_control,
				     start, end, to);
	return print_playlist_result(client, result);
}

enum command_return
handle_moveid(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned id;
	int to;
	enum playlist_result result;

	if (!check_unsigned(client, &id, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &to, argv[2]))
		return COMMAND_RETURN_ERROR;
	result = playlist_move_id(&g_playlist, client->player_control,
				  id, to);
	return print_playlist_result(client, result);
}

enum command_return
handle_swap(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned song1, song2;
	enum playlist_result result;

	if (!check_unsigned(client, &song1, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_unsigned(client, &song2, argv[2]))
		return COMMAND_RETURN_ERROR;
	result = playlist_swap_songs(&g_playlist, client->player_control,
				     song1, song2);
	return print_playlist_result(client, result);
}

enum command_return
handle_swapid(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned id1, id2;
	enum playlist_result result;

	if (!check_unsigned(client, &id1, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_unsigned(client, &id2, argv[2]))
		return COMMAND_RETURN_ERROR;
	result = playlist_swap_songs_id(&g_playlist, client->player_control,
					id1, id2);
	return print_playlist_result(client, result);
}
