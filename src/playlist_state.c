/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

/*
 * Saving and loading the playlist to/from the state file.
 *
 */

#include "playlist_state.h"
#include "playlist.h"
#include "player_control.h"
#include "queue_save.h"
#include "path.h"

#include <string.h>
#include <stdlib.h>

#define PLAYLIST_STATE_FILE_STATE		"state: "
#define PLAYLIST_STATE_FILE_RANDOM		"random: "
#define PLAYLIST_STATE_FILE_REPEAT		"repeat: "
#define PLAYLIST_STATE_FILE_SMARTSTOP		"smartstop: "
#define PLAYLIST_STATE_FILE_CURRENT		"current: "
#define PLAYLIST_STATE_FILE_TIME		"time: "
#define PLAYLIST_STATE_FILE_CROSSFADE		"crossfade: "
#define PLAYLIST_STATE_FILE_PLAYLIST_BEGIN	"playlist_begin"
#define PLAYLIST_STATE_FILE_PLAYLIST_END	"playlist_end"

#define PLAYLIST_STATE_FILE_STATE_PLAY		"play"
#define PLAYLIST_STATE_FILE_STATE_PAUSE		"pause"
#define PLAYLIST_STATE_FILE_STATE_STOP		"stop"

#define PLAYLIST_BUFFER_SIZE	2*MPD_PATH_MAX

void
playlist_state_save(FILE *fp, const struct playlist *playlist)
{
	fprintf(fp, "%s", PLAYLIST_STATE_FILE_STATE);

	if (playlist->playing) {
		switch (getPlayerState()) {
		case PLAYER_STATE_PAUSE:
			fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_STATE_PAUSE);
			break;
		default:
			fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_STATE_PLAY);
		}
		fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_CURRENT,
			queue_order_to_position(&playlist->queue,
						playlist->current));
		fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_TIME,
			getPlayerElapsedTime());
	} else
		fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_STATE_STOP);

	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_RANDOM,
		playlist->queue.random);
	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_REPEAT,
		playlist->queue.repeat);
	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_SMARTSTOP,
		playlist->queue.smartstop);
	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_CROSSFADE,
		(int)(getPlayerCrossFade()));
	fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_PLAYLIST_BEGIN);
	queue_save(fp, &playlist->queue);
	fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_PLAYLIST_END);
}

static void
playlist_state_load(FILE *fp, struct playlist *playlist, char *buffer)
{
	int song;

	if (!fgets(buffer, PLAYLIST_BUFFER_SIZE, fp)) {
		g_warning("No playlist in state file");
		return;
	}

	while (!g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_PLAYLIST_END)) {
		g_strchomp(buffer);

		song = queue_load_song(&playlist->queue, buffer);

		if (!fgets(buffer, PLAYLIST_BUFFER_SIZE, fp)) {
			g_warning("'%s' not found in state file",
				  PLAYLIST_STATE_FILE_PLAYLIST_END);
			break;
		}
	}

	queue_increment_version(&playlist->queue);
}

void
playlist_state_restore(FILE *fp, struct playlist *playlist)
{
	int current = -1;
	int seek_time = 0;
	int state = PLAYER_STATE_STOP;
	char buffer[PLAYLIST_BUFFER_SIZE];
	bool random_mode = false;

	while (fgets(buffer, sizeof(buffer), fp)) {
		g_strchomp(buffer);

		if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_STATE)) {
			if (strcmp(&(buffer[strlen(PLAYLIST_STATE_FILE_STATE)]),
				   PLAYLIST_STATE_FILE_STATE_PLAY) == 0) {
				state = PLAYER_STATE_PLAY;
			} else
			    if (strcmp
				(&(buffer[strlen(PLAYLIST_STATE_FILE_STATE)]),
				 PLAYLIST_STATE_FILE_STATE_PAUSE)
				== 0) {
				state = PLAYER_STATE_PAUSE;
			}
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_TIME)) {
			seek_time =
			    atoi(&(buffer[strlen(PLAYLIST_STATE_FILE_TIME)]));
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_REPEAT)) {
			if (strcmp
			    (&(buffer[strlen(PLAYLIST_STATE_FILE_REPEAT)]),
			     "1") == 0) {
				setPlaylistRepeatStatus(playlist, true);
			} else
				setPlaylistRepeatStatus(playlist, false);
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_SMARTSTOP)) {
			if (strcmp
			    (&(buffer[strlen(PLAYLIST_STATE_FILE_SMARTSTOP)]),
			     "1") == 0) {
				setPlaylistSmartstopStatus(playlist, true);
			} else
				setPlaylistSmartstopStatus(playlist, false);
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_CROSSFADE)) {
			setPlayerCrossFade(atoi
					   (&
					    (buffer
					     [strlen
					      (PLAYLIST_STATE_FILE_CROSSFADE)])));
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_RANDOM)) {
			random_mode =
				strcmp(buffer + strlen(PLAYLIST_STATE_FILE_RANDOM),
				       "1") == 0;
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_CURRENT)) {
			current = atoi(&(buffer
					 [strlen
					  (PLAYLIST_STATE_FILE_CURRENT)]));
		} else if (g_str_has_prefix(buffer,
					    PLAYLIST_STATE_FILE_PLAYLIST_BEGIN)) {
			if (state == PLAYER_STATE_STOP)
				current = -1;
			playlist_state_load(fp, playlist, buffer);
		}
	}

	setPlaylistRandomStatus(playlist, random_mode);

	if (state != PLAYER_STATE_STOP && !queue_is_empty(&playlist->queue)) {
		if (!queue_valid_position(&playlist->queue, current))
			current = 0;

		if (seek_time == 0)
			playPlaylist(playlist, current);
		else
			seekSongInPlaylist(playlist, current, seek_time);

		if (state == PLAYER_STATE_PAUSE)
			playerPause();
	}
}
