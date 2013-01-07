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
#include "PlayerCommands.hxx"
#include "CommandError.hxx"
#include "Playlist.hxx"
#include "PlaylistPrint.hxx"
#include "UpdateGlue.hxx"
#include "ClientInternal.hxx"
#include "Volume.hxx"
#include "OutputAll.hxx"
#include "protocol/Result.hxx"
#include "protocol/ArgParser.hxx"

extern "C" {
#include "audio_format.h"
}

#include "replay_gain_config.h"
#include "PlayerControl.hxx"

#include <errno.h>

#define COMMAND_STATUS_STATE            "state"
#define COMMAND_STATUS_REPEAT           "repeat"
#define COMMAND_STATUS_SINGLE           "single"
#define COMMAND_STATUS_CONSUME          "consume"
#define COMMAND_STATUS_RANDOM           "random"
#define COMMAND_STATUS_PLAYLIST         "playlist"
#define COMMAND_STATUS_PLAYLIST_LENGTH  "playlistlength"
#define COMMAND_STATUS_SONG             "song"
#define COMMAND_STATUS_SONGID           "songid"
#define COMMAND_STATUS_NEXTSONG         "nextsong"
#define COMMAND_STATUS_NEXTSONGID       "nextsongid"
#define COMMAND_STATUS_TIME             "time"
#define COMMAND_STATUS_BITRATE          "bitrate"
#define COMMAND_STATUS_ERROR            "error"
#define COMMAND_STATUS_CROSSFADE	"xfade"
#define COMMAND_STATUS_MIXRAMPDB	"mixrampdb"
#define COMMAND_STATUS_MIXRAMPDELAY	"mixrampdelay"
#define COMMAND_STATUS_AUDIO		"audio"
#define COMMAND_STATUS_UPDATING_DB	"updating_db"

enum command_return
handle_play(Client *client, int argc, char *argv[])
{
	int song = -1;
	enum playlist_result result;

	if (argc == 2 && !check_int(client, &song, argv[1]))
		return COMMAND_RETURN_ERROR;
	result = playlist_play(&client->playlist, client->player_control,
			       song);
	return print_playlist_result(client, result);
}

enum command_return
handle_playid(Client *client, int argc, char *argv[])
{
	int id = -1;
	enum playlist_result result;

	if (argc == 2 && !check_int(client, &id, argv[1]))
		return COMMAND_RETURN_ERROR;

	result = playlist_play_id(&client->playlist, client->player_control,
				  id);
	return print_playlist_result(client, result);
}

enum command_return
handle_stop(Client *client,
	    G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	playlist_stop(&client->playlist, client->player_control);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_currentsong(Client *client,
		   G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	playlist_print_current(client, &client->playlist);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_pause(Client *client,
	     int argc, char *argv[])
{
	if (argc == 2) {
		bool pause_flag;
		if (!check_bool(client, &pause_flag, argv[1]))
			return COMMAND_RETURN_ERROR;

		pc_set_pause(client->player_control, pause_flag);
	} else
		pc_pause(client->player_control);

	return COMMAND_RETURN_OK;
}

enum command_return
handle_status(Client *client,
	      G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	const char *state = NULL;
	struct player_status player_status;
	int updateJobId;
	char *error;
	int song;

	pc_get_status(client->player_control, &player_status);

	switch (player_status.state) {
	case PLAYER_STATE_STOP:
		state = "stop";
		break;
	case PLAYER_STATE_PAUSE:
		state = "pause";
		break;
	case PLAYER_STATE_PLAY:
		state = "play";
		break;
	}

	const playlist &playlist = client->playlist;
	client_printf(client,
		      "volume: %i\n"
		      COMMAND_STATUS_REPEAT ": %i\n"
		      COMMAND_STATUS_RANDOM ": %i\n"
		      COMMAND_STATUS_SINGLE ": %i\n"
		      COMMAND_STATUS_CONSUME ": %i\n"
		      COMMAND_STATUS_PLAYLIST ": %li\n"
		      COMMAND_STATUS_PLAYLIST_LENGTH ": %i\n"
		      COMMAND_STATUS_CROSSFADE ": %i\n"
		      COMMAND_STATUS_MIXRAMPDB ": %f\n"
		      COMMAND_STATUS_MIXRAMPDELAY ": %f\n"
		      COMMAND_STATUS_STATE ": %s\n",
		      volume_level_get(),
		      playlist_get_repeat(&playlist),
		      playlist_get_random(&playlist),
		      playlist_get_single(&playlist),
		      playlist_get_consume(&playlist),
		      playlist_get_version(&playlist),
		      playlist_get_length(&playlist),
		      (int)(pc_get_cross_fade(client->player_control) + 0.5),
		      pc_get_mixramp_db(client->player_control),
		      pc_get_mixramp_delay(client->player_control),
		      state);

	song = playlist_get_current_song(&playlist);
	if (song >= 0) {
		client_printf(client,
			      COMMAND_STATUS_SONG ": %i\n"
			      COMMAND_STATUS_SONGID ": %u\n",
			      song, playlist_get_song_id(&playlist, song));
	}

	if (player_status.state != PLAYER_STATE_STOP) {
		struct audio_format_string af_string;

		client_printf(client,
			      COMMAND_STATUS_TIME ": %i:%i\n"
			      "elapsed: %1.3f\n"
			      COMMAND_STATUS_BITRATE ": %u\n"
			      COMMAND_STATUS_AUDIO ": %s\n",
			      (int)(player_status.elapsed_time + 0.5),
			      (int)(player_status.total_time + 0.5),
			      player_status.elapsed_time,
			      player_status.bit_rate,
			      audio_format_to_string(&player_status.audio_format,
						     &af_string));
	}

	if ((updateJobId = isUpdatingDB())) {
		client_printf(client,
			      COMMAND_STATUS_UPDATING_DB ": %i\n",
			      updateJobId);
	}

	error = pc_get_error_message(client->player_control);
	if (error != NULL) {
		client_printf(client,
			      COMMAND_STATUS_ERROR ": %s\n",
			      error);
		g_free(error);
	}

	song = playlist_get_next_song(&playlist);
	if (song >= 0) {
		client_printf(client,
			      COMMAND_STATUS_NEXTSONG ": %i\n"
			      COMMAND_STATUS_NEXTSONGID ": %u\n",
			      song, playlist_get_song_id(&playlist, song));
	}

	return COMMAND_RETURN_OK;
}

enum command_return
handle_next(Client *client,
	    G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	playlist &playlist = client->playlist;

	/* single mode is not considered when this is user who
	 * wants to change song. */
	const bool single = playlist.queue.single;
	playlist.queue.single = false;

	playlist_next(&playlist, client->player_control);

	playlist.queue.single = single;
	return COMMAND_RETURN_OK;
}

enum command_return
handle_previous(Client *client,
		G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	playlist_previous(&client->playlist, client->player_control);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_repeat(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	bool status;
	if (!check_bool(client, &status, argv[1]))
		return COMMAND_RETURN_ERROR;

	playlist_set_repeat(&client->playlist, client->player_control, status);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_single(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	bool status;
	if (!check_bool(client, &status, argv[1]))
		return COMMAND_RETURN_ERROR;

	playlist_set_single(&client->playlist, client->player_control, status);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_consume(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	bool status;
	if (!check_bool(client, &status, argv[1]))
		return COMMAND_RETURN_ERROR;

	playlist_set_consume(&client->playlist, status);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_random(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	bool status;
	if (!check_bool(client, &status, argv[1]))
		return COMMAND_RETURN_ERROR;

	playlist_set_random(&client->playlist, client->player_control, status);
	audio_output_all_set_replay_gain_mode(replay_gain_get_real_mode(client->playlist.queue.random));
	return COMMAND_RETURN_OK;
}

enum command_return
handle_clearerror(G_GNUC_UNUSED Client *client,
		  G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	pc_clear_error(client->player_control);
	return COMMAND_RETURN_OK;
}

enum command_return
handle_seek(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned song, seek_time;
	enum playlist_result result;

	if (!check_unsigned(client, &song, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_unsigned(client, &seek_time, argv[2]))
		return COMMAND_RETURN_ERROR;

	result = playlist_seek_song(&client->playlist, client->player_control,
				    song, seek_time);
	return print_playlist_result(client, result);
}

enum command_return
handle_seekid(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned id, seek_time;
	enum playlist_result result;

	if (!check_unsigned(client, &id, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_unsigned(client, &seek_time, argv[2]))
		return COMMAND_RETURN_ERROR;

	result = playlist_seek_song_id(&client->playlist,
				       client->player_control,
				       id, seek_time);
	return print_playlist_result(client, result);
}

enum command_return
handle_seekcur(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	const char *p = argv[1];
	bool relative = *p == '+' || *p == '-';
	int seek_time;
	if (!check_int(client, &seek_time, p))
		return COMMAND_RETURN_ERROR;

	enum playlist_result result =
		playlist_seek_current(&client->playlist,
				      client->player_control,
				      seek_time, relative);
	return print_playlist_result(client, result);
}

enum command_return
handle_crossfade(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned xfade_time;

	if (!check_unsigned(client, &xfade_time, argv[1]))
		return COMMAND_RETURN_ERROR;
	pc_set_cross_fade(client->player_control, xfade_time);

	return COMMAND_RETURN_OK;
}

enum command_return
handle_mixrampdb(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	float db;

	if (!check_float(client, &db, argv[1]))
		return COMMAND_RETURN_ERROR;
	pc_set_mixramp_db(client->player_control, db);

	return COMMAND_RETURN_OK;
}

enum command_return
handle_mixrampdelay(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	float delay_secs;

	if (!check_float(client, &delay_secs, argv[1]))
		return COMMAND_RETURN_ERROR;
	pc_set_mixramp_delay(client->player_control, delay_secs);

	return COMMAND_RETURN_OK;
}

enum command_return
handle_replay_gain_mode(Client *client,
			G_GNUC_UNUSED int argc, char *argv[])
{
	if (!replay_gain_set_mode_string(argv[1])) {
		command_error(client, ACK_ERROR_ARG,
			      "Unrecognized replay gain mode");
		return COMMAND_RETURN_ERROR;
	}

	audio_output_all_set_replay_gain_mode(replay_gain_get_real_mode(client->playlist.queue.random));

	return COMMAND_RETURN_OK;
}

enum command_return
handle_replay_gain_status(Client *client,
			  G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	client_printf(client, "replay_gain_mode: %s\n",
		      replay_gain_get_mode_string());
	return COMMAND_RETURN_OK;
}
