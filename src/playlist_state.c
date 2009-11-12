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

#include "config.h"
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
#define PLAYLIST_STATE_FILE_SINGLE		"single: "
#define PLAYLIST_STATE_FILE_CONSUME		"consume: "
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
	struct player_status player_status;

	pc_get_status(&player_status);

	fprintf(fp, "%s", PLAYLIST_STATE_FILE_STATE);

	if (playlist->playing) {
		switch (player_status.state) {
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
			(int)player_status.elapsed_time);
	} else {
		fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_STATE_STOP);

		if (playlist->current >= 0)
			fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_CURRENT,
				queue_order_to_position(&playlist->queue,
							playlist->current));
	}

	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_RANDOM,
		playlist->queue.random);
	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_REPEAT,
		playlist->queue.repeat);
	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_SINGLE,
		playlist->queue.single);
	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_CONSUME,
		playlist->queue.consume);
	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_CROSSFADE,
		(int)(pc_get_cross_fade()));
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

bool
playlist_state_restore(const char *line, FILE *fp, struct playlist *playlist)
{
	int current = -1;
	int seek_time = 0;
	int state = PLAYER_STATE_STOP;
	char buffer[PLAYLIST_BUFFER_SIZE];
	bool random_mode = false;

	if (!g_str_has_prefix(line, PLAYLIST_STATE_FILE_STATE))
		return false;

	line += sizeof(PLAYLIST_STATE_FILE_STATE) - 1;

	if (strcmp(line, PLAYLIST_STATE_FILE_STATE_PLAY) == 0)
		state = PLAYER_STATE_PLAY;
	else if (strcmp(line, PLAYLIST_STATE_FILE_STATE_PAUSE) == 0)
		state = PLAYER_STATE_PAUSE;

	while (fgets(buffer, sizeof(buffer), fp)) {
		g_strchomp(buffer);

		if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_TIME)) {
			seek_time =
			    atoi(&(buffer[strlen(PLAYLIST_STATE_FILE_TIME)]));
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_REPEAT)) {
			if (strcmp
			    (&(buffer[strlen(PLAYLIST_STATE_FILE_REPEAT)]),
			     "1") == 0) {
				playlist_set_repeat(playlist, true);
			} else
				playlist_set_repeat(playlist, false);
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_SINGLE)) {
			if (strcmp
			    (&(buffer[strlen(PLAYLIST_STATE_FILE_SINGLE)]),
			     "1") == 0) {
				playlist_set_single(playlist, true);
			} else
				playlist_set_single(playlist, false);
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_CONSUME)) {
			if (strcmp
			    (&(buffer[strlen(PLAYLIST_STATE_FILE_CONSUME)]),
			     "1") == 0) {
				playlist_set_consume(playlist, true);
			} else
				playlist_set_consume(playlist, false);
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_CROSSFADE)) {
			pc_set_cross_fade(atoi(buffer + strlen(PLAYLIST_STATE_FILE_CROSSFADE)));
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
			playlist_state_load(fp, playlist, buffer);
		}
	}

	playlist_set_random(playlist, random_mode);

	if (!queue_is_empty(&playlist->queue)) {
		if (!queue_valid_position(&playlist->queue, current))
			current = 0;

		/* enable all devices for the first time; this must be
		   called here, after the audio output states were
		   restored, before playback begins */
		if (state != PLAYER_STATE_STOP)
			pc_update_audio();

		if (state == PLAYER_STATE_STOP /* && config_option */)
			playlist->current = current;
		else if (seek_time == 0)
			playlist_play(playlist, current);
		else
			playlist_seek_song(playlist, current, seek_time);

		if (state == PLAYER_STATE_PAUSE)
			pc_pause();
	}

	return true;
}

unsigned
playlist_state_get_hash(const struct playlist *playlist)
{
	struct player_status player_status;

	pc_get_status(&player_status);

	return playlist->queue.version ^
		(player_status.state != PLAYER_STATE_STOP
		 ? ((int)player_status.elapsed_time << 8)
		 : 0) ^
		(playlist->current >= 0
		 ? (queue_order_to_position(&playlist->queue,
					    playlist->current) << 16)
		 : 0) ^
		((int)pc_get_cross_fade() << 20) ^
		(player_status.state << 24) ^
		(playlist->queue.random << 27) ^
		(playlist->queue.repeat << 28) ^
		(playlist->queue.single << 29) ^
		(playlist->queue.consume << 30) ^
		(playlist->queue.random << 31);
}
