/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "Request.hxx"
#include "CommandError.hxx"
#include "queue/Playlist.hxx"
#include "PlaylistPrint.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "mixer/Volume.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "Idle.hxx"
#include "AudioFormat.hxx"
#include "ReplayGainConfig.hxx"
#include "util/ScopeExit.hxx"
#include "util/Error.hxx"

#ifdef ENABLE_DATABASE
#include "db/update/Service.hxx"
#endif

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

CommandResult
handle_play(Client &client, Request args, gcc_unused Response &r)
{
	int song = args.ParseOptional(0, -1);

	client.partition.PlayPosition(song);
	return CommandResult::OK;
}

CommandResult
handle_playid(Client &client, Request args, gcc_unused Response &r)
{
	int id = args.ParseOptional(0, -1);

	client.partition.PlayId(id);
	return CommandResult::OK;
}

CommandResult
handle_stop(Client &client, gcc_unused Request args, gcc_unused Response &r)
{
	client.partition.Stop();
	return CommandResult::OK;
}

CommandResult
handle_currentsong(Client &client, gcc_unused Request args, Response &r)
{
	playlist_print_current(r, client.partition, client.playlist);
	return CommandResult::OK;
}

CommandResult
handle_pause(Client &client, Request args, gcc_unused Response &r)
{
	if (!args.IsEmpty()) {
		bool pause_flag = args.ParseBool(0);
		client.player_control.LockSetPause(pause_flag);
	} else
		client.player_control.LockPause();

	return CommandResult::OK;
}

CommandResult
handle_status(Client &client, gcc_unused Request args, Response &r)
{
	const char *state = nullptr;
	int song;

	const auto player_status = client.player_control.LockGetStatus();

	switch (player_status.state) {
	case PlayerState::STOP:
		state = "stop";
		break;
	case PlayerState::PAUSE:
		state = "pause";
		break;
	case PlayerState::PLAY:
		state = "play";
		break;
	}

	const playlist &playlist = client.playlist;
	r.Format("volume: %i\n"
		 COMMAND_STATUS_REPEAT ": %i\n"
		 COMMAND_STATUS_RANDOM ": %i\n"
		 COMMAND_STATUS_SINGLE ": %i\n"
		 COMMAND_STATUS_CONSUME ": %i\n"
		 COMMAND_STATUS_PLAYLIST ": %li\n"
		 COMMAND_STATUS_PLAYLIST_LENGTH ": %i\n"
		 COMMAND_STATUS_MIXRAMPDB ": %f\n"
		 COMMAND_STATUS_STATE ": %s\n",
		 volume_level_get(client.partition.outputs),
		 playlist.GetRepeat(),
		 playlist.GetRandom(),
		 playlist.GetSingle(),
		 playlist.GetConsume(),
		 (unsigned long)playlist.GetVersion(),
		 playlist.GetLength(),
		 client.player_control.GetMixRampDb(),
		 state);

	if (client.player_control.GetCrossFade() > 0)
		r.Format(COMMAND_STATUS_CROSSFADE ": %i\n",
			 int(client.player_control.GetCrossFade() + 0.5));

	if (client.player_control.GetMixRampDelay() > 0)
		r.Format(COMMAND_STATUS_MIXRAMPDELAY ": %f\n",
			 client.player_control.GetMixRampDelay());

	song = playlist.GetCurrentPosition();
	if (song >= 0) {
		r.Format(COMMAND_STATUS_SONG ": %i\n"
			 COMMAND_STATUS_SONGID ": %u\n",
			 song, playlist.PositionToId(song));
	}

	if (player_status.state != PlayerState::STOP) {
		r.Format(COMMAND_STATUS_TIME ": %i:%i\n"
			 "elapsed: %1.3f\n"
			 COMMAND_STATUS_BITRATE ": %u\n",
			 player_status.elapsed_time.RoundS(),
			 player_status.total_time.IsNegative()
			 ? 0u
			 : unsigned(player_status.total_time.RoundS()),
			 player_status.elapsed_time.ToDoubleS(),
			 player_status.bit_rate);

		if (!player_status.total_time.IsNegative())
			r.Format("duration: %1.3f\n",
				 player_status.total_time.ToDoubleS());

		if (player_status.audio_format.IsDefined()) {
			struct audio_format_string af_string;

			r.Format(COMMAND_STATUS_AUDIO ": %s\n",
				 audio_format_to_string(player_status.audio_format,
							&af_string));
		}
	}

#ifdef ENABLE_DATABASE
	const UpdateService *update_service = client.partition.instance.update;
	unsigned updateJobId = update_service != nullptr
		? update_service->GetId()
		: 0;
	if (updateJobId != 0) {
		r.Format(COMMAND_STATUS_UPDATING_DB ": %i\n",
			 updateJobId);
	}
#endif

	try {
		client.player_control.LockCheckRethrowError();
	} catch (const std::exception &e) {
		r.Format(COMMAND_STATUS_ERROR ": %s\n", e.what());
	} catch (const Error &error) {
		r.Format(COMMAND_STATUS_ERROR ": %s\n", error.GetMessage());
	} catch (...) {
		r.Format(COMMAND_STATUS_ERROR ": unknown\n");
	}

	song = playlist.GetNextPosition();
	if (song >= 0)
		r.Format(COMMAND_STATUS_NEXTSONG ": %i\n"
			 COMMAND_STATUS_NEXTSONGID ": %u\n",
			 song, playlist.PositionToId(song));

	return CommandResult::OK;
}

CommandResult
handle_next(Client &client, gcc_unused Request args, gcc_unused Response &r)
{
	playlist &playlist = client.playlist;

	/* single mode is not considered when this is user who
	 * wants to change song. */
	const bool single = playlist.queue.single;
	playlist.queue.single = false;

	AtScopeExit(&playlist, single) {
		playlist.queue.single = single;
	};

	client.partition.PlayNext();
	return CommandResult::OK;
}

CommandResult
handle_previous(Client &client, gcc_unused Request args,
		gcc_unused Response &r)
{
	client.partition.PlayPrevious();
	return CommandResult::OK;
}

CommandResult
handle_repeat(Client &client, Request args, gcc_unused Response &r)
{
	bool status = args.ParseBool(0);
	client.partition.SetRepeat(status);
	return CommandResult::OK;
}

CommandResult
handle_single(Client &client, Request args, gcc_unused Response &r)
{
	bool status = args.ParseBool(0);
	client.partition.SetSingle(status);
	return CommandResult::OK;
}

CommandResult
handle_consume(Client &client, Request args, gcc_unused Response &r)
{
	bool status = args.ParseBool(0);
	client.partition.SetConsume(status);
	return CommandResult::OK;
}

CommandResult
handle_random(Client &client, Request args, gcc_unused Response &r)
{
	bool status = args.ParseBool(0);
	client.partition.SetRandom(status);
	client.partition.outputs.SetReplayGainMode(replay_gain_get_real_mode(client.partition.GetRandom()));
	return CommandResult::OK;
}

CommandResult
handle_clearerror(Client &client, gcc_unused Request args,
		  gcc_unused Response &r)
{
	client.player_control.LockClearError();
	return CommandResult::OK;
}

CommandResult
handle_seek(Client &client, Request args, gcc_unused Response &r)
{
	unsigned song = args.ParseUnsigned(0);
	SongTime seek_time = args.ParseSongTime(1);

	client.partition.SeekSongPosition(song, seek_time);
	return CommandResult::OK;
}

CommandResult
handle_seekid(Client &client, Request args, gcc_unused Response &r)
{
	unsigned id = args.ParseUnsigned(0);
	SongTime seek_time = args.ParseSongTime(1);

	client.partition.SeekSongId(id, seek_time);
	return CommandResult::OK;
}

CommandResult
handle_seekcur(Client &client, Request args, gcc_unused Response &r)
{
	const char *p = args.front();
	bool relative = *p == '+' || *p == '-';
	SignedSongTime seek_time = ParseCommandArgSignedSongTime(p);

	client.partition.SeekCurrent(seek_time, relative);
	return CommandResult::OK;
}

CommandResult
handle_crossfade(Client &client, Request args, gcc_unused Response &r)
{
	unsigned xfade_time = args.ParseUnsigned(0);
	client.player_control.SetCrossFade(xfade_time);
	return CommandResult::OK;
}

CommandResult
handle_mixrampdb(Client &client, Request args, gcc_unused Response &r)
{
	float db = args.ParseFloat(0);
	client.player_control.SetMixRampDb(db);
	return CommandResult::OK;
}

CommandResult
handle_mixrampdelay(Client &client, Request args, gcc_unused Response &r)
{
	float delay_secs = args.ParseFloat(0);
	client.player_control.SetMixRampDelay(delay_secs);
	return CommandResult::OK;
}

CommandResult
handle_replay_gain_mode(Client &client, Request args, Response &r)
{
	if (!replay_gain_set_mode_string(args.front())) {
		r.Error(ACK_ERROR_ARG, "Unrecognized replay gain mode");
		return CommandResult::ERROR;
	}

	client.partition.outputs.SetReplayGainMode(replay_gain_get_real_mode(client.playlist.queue.random));
	client.partition.EmitIdle(IDLE_OPTIONS);
	return CommandResult::OK;
}

CommandResult
handle_replay_gain_status(gcc_unused Client &client, gcc_unused Request args,
			  Response &r)
{
	r.Format("replay_gain_mode: %s\n", replay_gain_get_mode_string());
	return CommandResult::OK;
}
