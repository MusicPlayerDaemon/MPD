// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Saving and loading the playlist to/from the state file.
 *
 */

#include "PlaylistState.hxx"
#include "PlaylistError.hxx"
#include "Playlist.hxx"
#include "SingleMode.hxx"
#include "StateFileConfig.hxx"
#include "Save.hxx"
#include "io/LineReader.hxx"
#include "io/BufferedOutputStream.hxx"
#include "player/Control.hxx"
#include "util/CharUtil.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "util/NumberParser.hxx"
#include "Log.hxx"

#include <fmt/format.h>

#include <string.h>
#include <stdlib.h>

#define PLAYLIST_STATE_FILE_STATE		"state: "
#define PLAYLIST_STATE_FILE_RANDOM		"random: "
#define PLAYLIST_STATE_FILE_REPEAT		"repeat: "
#define PLAYLIST_STATE_FILE_SINGLE		"single: "
#define PLAYLIST_STATE_FILE_CONSUME		"consume: "
#define PLAYLIST_STATE_FILE_CURRENT		"current: "
#define PLAYLIST_STATE_FILE_TIME		"time: "
#define PLAYLIST_STATE_FILE_CROSSFADE		"crossfade: "
#define PLAYLIST_STATE_FILE_MIXRAMPDB		"mixrampdb: "
#define PLAYLIST_STATE_FILE_MIXRAMPDELAY	"mixrampdelay: "
#define PLAYLIST_STATE_FILE_PLAYLIST_BEGIN	"playlist_begin"
#define PLAYLIST_STATE_FILE_PLAYLIST_END	"playlist_end"

#define PLAYLIST_STATE_FILE_STATE_PLAY		"play"
#define PLAYLIST_STATE_FILE_STATE_PAUSE		"pause"
#define PLAYLIST_STATE_FILE_STATE_STOP		"stop"

void
playlist_state_save(BufferedOutputStream &os, const struct playlist &playlist,
		    PlayerControl &pc)
{
	const auto player_status = pc.LockGetStatus();

	os.Write(PLAYLIST_STATE_FILE_STATE);

	if (playlist.playing) {
		switch (player_status.state) {
		case PlayerState::PAUSE:
			os.Write(PLAYLIST_STATE_FILE_STATE_PAUSE "\n");
			break;
		default:
			os.Write(PLAYLIST_STATE_FILE_STATE_PLAY "\n");
		}
		os.Fmt(FMT_STRING(PLAYLIST_STATE_FILE_CURRENT "{}\n"),
		       playlist.queue.OrderToPosition(playlist.current));
		os.Fmt(FMT_STRING(PLAYLIST_STATE_FILE_TIME "{}\n"),
		       player_status.elapsed_time.ToDoubleS());
	} else {
		os.Write(PLAYLIST_STATE_FILE_STATE_STOP "\n");

		if (playlist.current >= 0)
			os.Fmt(FMT_STRING(PLAYLIST_STATE_FILE_CURRENT "{}\n"),
			       playlist.queue.OrderToPosition(playlist.current));
	}

	os.Fmt(FMT_STRING(PLAYLIST_STATE_FILE_RANDOM "{}\n"),
	       (unsigned)playlist.queue.random);
	os.Fmt(FMT_STRING(PLAYLIST_STATE_FILE_REPEAT "{}\n"),
	       (unsigned)playlist.queue.repeat);
	os.Fmt(FMT_STRING(PLAYLIST_STATE_FILE_SINGLE "{}\n"),
		   SingleToString(playlist.queue.single));
	os.Fmt(FMT_STRING(PLAYLIST_STATE_FILE_CONSUME "{}\n"),
	       ConsumeToString(playlist.queue.consume));
	os.Fmt(FMT_STRING(PLAYLIST_STATE_FILE_CROSSFADE "{}\n"),
	       pc.GetCrossFade().count());
	os.Fmt(FMT_STRING(PLAYLIST_STATE_FILE_MIXRAMPDB "{}\n"),
	       pc.GetMixRampDb());
	os.Fmt(FMT_STRING(PLAYLIST_STATE_FILE_MIXRAMPDELAY "{}\n"),
	       pc.GetMixRampDelay().count());
	os.Write(PLAYLIST_STATE_FILE_PLAYLIST_BEGIN "\n");
	queue_save(os, playlist.queue);
	os.Write(PLAYLIST_STATE_FILE_PLAYLIST_END "\n");
}

static void
playlist_state_load(LineReader &file, const SongLoader &song_loader,
		    struct playlist &playlist)
{
	const char *line = file.ReadLine();
	if (line == nullptr) {
		LogWarning(playlist_domain, "No playlist in state file");
		return;
	}

	while (!StringStartsWith(line, PLAYLIST_STATE_FILE_PLAYLIST_END)) {
		queue_load_song(file, song_loader, line, playlist.queue);

		line = file.ReadLine();
		if (line == nullptr) {
			LogWarning(playlist_domain,
				   "'" PLAYLIST_STATE_FILE_PLAYLIST_END
				   "' not found in state file");
			break;
		}
	}

	playlist.queue.IncrementVersion();
}

bool
playlist_state_restore(const StateFileConfig &config,
		       const char *line, LineReader &file,
		       const SongLoader &song_loader,
		       struct playlist &playlist, PlayerControl &pc)
{
	int current = -1;
	SongTime seek_time = SongTime::zero();
	bool random_mode = false;

	line = StringAfterPrefix(line, PLAYLIST_STATE_FILE_STATE);
	if (line == nullptr)
		return false;

	PlayerState state;
	if (strcmp(line, PLAYLIST_STATE_FILE_STATE_PLAY) == 0)
		state = PlayerState::PLAY;
	else if (strcmp(line, PLAYLIST_STATE_FILE_STATE_PAUSE) == 0)
		state = PlayerState::PAUSE;
	else
		state = PlayerState::STOP;

	while ((line = file.ReadLine()) != nullptr) {
		const char *p;
		if ((p = StringAfterPrefix(line, PLAYLIST_STATE_FILE_TIME))) {
			seek_time = SongTime::FromS(ParseDouble(p));
		} else if ((p = StringAfterPrefix(line, PLAYLIST_STATE_FILE_REPEAT))) {
			playlist.SetRepeat(pc, StringIsEqual(p, "1"));
		} else if ((p = StringAfterPrefix(line, PLAYLIST_STATE_FILE_SINGLE))) {
			playlist.SetSingle(pc, SingleFromString(p));
		} else if ((p = StringAfterPrefix(line, PLAYLIST_STATE_FILE_CONSUME))) {
			playlist.SetConsume(ConsumeFromString(p));
		} else if ((p = StringAfterPrefix(line, PLAYLIST_STATE_FILE_CROSSFADE))) {
			pc.SetCrossFade(FloatDuration(atoi(p)));
		} else if ((p = StringAfterPrefix(line, PLAYLIST_STATE_FILE_MIXRAMPDB))) {
			pc.SetMixRampDb(ParseFloat(p));
		} else if ((p = StringAfterPrefix(line, PLAYLIST_STATE_FILE_MIXRAMPDELAY))) {
			/* this check discards "nan" which was used
			   prior to MPD 0.18 */
			if (IsDigitASCII(*p))
				pc.SetMixRampDelay(FloatDuration(ParseFloat(p)));
		} else if ((p = StringAfterPrefix(line, PLAYLIST_STATE_FILE_RANDOM))) {
			random_mode = StringIsEqual(p, "1");
		} else if ((p = StringAfterPrefix(line, PLAYLIST_STATE_FILE_CURRENT))) {
			current = atoi(p);
		} else if (StringStartsWith(line,
					    PLAYLIST_STATE_FILE_PLAYLIST_BEGIN)) {
			playlist_state_load(file, song_loader, playlist);
		}
	}

	playlist.SetRandom(pc, random_mode);

	if (!playlist.queue.IsEmpty()) {
		if (!playlist.queue.IsValidPosition(current))
			current = 0;

		if (state == PlayerState::PLAY && config.restore_paused)
			/* the user doesn't want MPD to auto-start
			   playback after startup; fall back to
			   "pause" */
			state = PlayerState::PAUSE;

		/* enable all devices for the first time; this must be
		   called here, after the audio output states were
		   restored, before playback begins */
		if (state != PlayerState::STOP)
			pc.LockUpdateAudio();

		if (state == PlayerState::STOP /* && config_option */)
			playlist.current = current;
		else if (seek_time.count() == 0) {
			try {
				playlist.PlayPosition(pc, current);
			} catch (...) {
				/* TODO: log error? */
			}
		} else {
			try {
				playlist.SeekSongPosition(pc, current,
							  seek_time);
			} catch (...) {
				/* TODO: log error? */
			}
		}

		if (state == PlayerState::PAUSE)
			pc.LockPause();
	}

	return true;
}

unsigned
playlist_state_get_hash(const playlist &playlist,
			PlayerControl &pc)
{
	const auto player_status = pc.LockGetStatus();

	return playlist.queue.version ^
		(player_status.state != PlayerState::STOP
		 ? (player_status.elapsed_time.ToS() << 8)
		 : 0) ^
		(playlist.current >= 0
		 ? (playlist.queue.OrderToPosition(playlist.current) << 16)
		 : 0) ^
		((int)pc.GetCrossFade().count() << 20) ^
		(unsigned(player_status.state) << 24) ^
		/* note that this takes 2 bits */
		((int)playlist.queue.single << 25) ^
		(playlist.queue.random << 27) ^
		(playlist.queue.repeat << 28) ^
		/* note that this takes 2 bits */
		((int)playlist.queue.consume << 29) ^
		(playlist.queue.random << 31);
}
