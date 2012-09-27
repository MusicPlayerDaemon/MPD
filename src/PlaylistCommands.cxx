/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "DatabasePlaylist.hxx"
#include "CommandError.hxx"
#include "PlaylistPrint.hxx"
#include "PlaylistSave.hxx"
#include "PlaylistFile.h"

extern "C" {
#include "protocol/argparser.h"
#include "protocol/result.h"
#include "playlist.h"
#include "playlist_queue.h"
#include "time_print.h"
#include "ls.h"
#include "uri.h"
#include "client_internal.h"
}

#include <assert.h>
#include <stdlib.h>

static void
print_spl_list(struct client *client, GPtrArray *list)
{
	for (unsigned i = 0; i < list->len; ++i) {
		struct stored_playlist_info *playlist =
			(struct stored_playlist_info *)
			g_ptr_array_index(list, i);

		client_printf(client, "playlist: %s\n", playlist->name);

		if (playlist->mtime > 0)
			time_print(client, "Last-Modified", playlist->mtime);
	}
}

enum command_return
handle_save(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	enum playlist_result result;

	result = spl_save_playlist(argv[1], &g_playlist);
	return print_playlist_result(client, result);
}

enum command_return
handle_load(struct client *client, int argc, char *argv[])
{
	unsigned start_index, end_index;

	if (argc < 3) {
		start_index = 0;
		end_index = G_MAXUINT;
	} else if (!check_range(client, &start_index, &end_index, argv[2]))
		return COMMAND_RETURN_ERROR;

	enum playlist_result result;

	result = playlist_open_into_queue(argv[1],
					  start_index, end_index,
					  &g_playlist,
					  client->player_control, true);
	if (result != PLAYLIST_RESULT_NO_SUCH_LIST)
		return print_playlist_result(client, result);

	GError *error = NULL;
	if (playlist_load_spl(&g_playlist, client->player_control,
			      argv[1], start_index, end_index,
			      &error))
		return COMMAND_RETURN_OK;

	if (error->domain == playlist_quark() &&
	    error->code == PLAYLIST_RESULT_BAD_NAME)
		/* the message for BAD_NAME is confusing when the
		   client wants to load a playlist file from the music
		   directory; patch the GError object to show "no such
		   playlist" instead */
		error->code = PLAYLIST_RESULT_NO_SUCH_LIST;

	return print_error(client, error);
}

enum command_return
handle_listplaylist(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	if (playlist_file_print(client, argv[1], false))
		return COMMAND_RETURN_OK;

	GError *error = NULL;
	return spl_print(client, argv[1], false, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);
}

enum command_return
handle_listplaylistinfo(struct client *client,
			G_GNUC_UNUSED int argc, char *argv[])
{
	if (playlist_file_print(client, argv[1], true))
		return COMMAND_RETURN_OK;

	GError *error = NULL;
	return spl_print(client, argv[1], true, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);
}

enum command_return
handle_rm(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	GError *error = NULL;
	return spl_delete(argv[1], &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);
}

enum command_return
handle_rename(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	GError *error = NULL;
	return spl_rename(argv[1], argv[2], &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);
}

enum command_return
handle_playlistdelete(struct client *client,
		      G_GNUC_UNUSED int argc, char *argv[]) {
	char *playlist = argv[1];
	unsigned from;

	if (!check_unsigned(client, &from, argv[2]))
		return COMMAND_RETURN_ERROR;

	GError *error = NULL;
	return spl_remove_index(playlist, from, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);
}

enum command_return
handle_playlistmove(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	char *playlist = argv[1];
	unsigned from, to;

	if (!check_unsigned(client, &from, argv[2]))
		return COMMAND_RETURN_ERROR;
	if (!check_unsigned(client, &to, argv[3]))
		return COMMAND_RETURN_ERROR;

	GError *error = NULL;
	return spl_move_index(playlist, from, to, &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);
}

enum command_return
handle_playlistclear(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	GError *error = NULL;
	return spl_clear(argv[1], &error)
		? COMMAND_RETURN_OK
		: print_error(client, error);
}

enum command_return
handle_playlistadd(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	char *playlist = argv[1];
	char *uri = argv[2];

	bool success;
	GError *error = NULL;
	if (uri_has_scheme(uri)) {
		if (!uri_supported_scheme(uri)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported URI scheme");
			return COMMAND_RETURN_ERROR;
		}

		success = spl_append_uri(argv[1], playlist, &error);
	} else
		success = search_add_to_playlist(uri, playlist, nullptr,
						 &error);

	if (!success && error == NULL) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");
		return COMMAND_RETURN_ERROR;
	}

	return success ? COMMAND_RETURN_OK : print_error(client, error);
}

enum command_return
handle_listplaylists(struct client *client,
		     G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	GError *error = NULL;
	GPtrArray *list = spl_list(&error);
	if (list == NULL)
		return print_error(client, error);

	print_spl_list(client, list);
	spl_list_free(list);
	return COMMAND_RETURN_OK;
}
