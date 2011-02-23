/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "text_file.h"
#include "conf.h"

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

#define PLAYLIST_BUFFER_SIZE	2*MPD_PATH_MAX

void
playlist_state_save(FILE *fp, const struct playlist *playlist,
		    struct player_control *pc)
{
	struct player_status player_status;

	pc_get_status(pc, &player_status);

	fputs(PLAYLIST_STATE_FILE_STATE, fp);

	if (playlist->playing) {
		switch (player_status.state) {
		case PLAYER_STATE_PAUSE:
			fputs(PLAYLIST_STATE_FILE_STATE_PAUSE "\n", fp);
			break;
		default:
			fputs(PLAYLIST_STATE_FILE_STATE_PLAY "\n", fp);
		}
		fprintf(fp, PLAYLIST_STATE_FILE_CURRENT "%i\n",
			queue_order_to_position(&playlist->queue,
						playlist->current));
		fprintf(fp, PLAYLIST_STATE_FILE_TIME "%i\n",
			(int)player_status.elapsed_time);
	} else {
		fputs(PLAYLIST_STATE_FILE_STATE_STOP "\n", fp);

		if (playlist->current >= 0)
			fprintf(fp, PLAYLIST_STATE_FILE_CURRENT "%i\n",
				queue_order_to_position(&playlist->queue,
							playlist->current));
	}

	fprintf(fp, PLAYLIST_STATE_FILE_RANDOM "%i\n", playlist->queue.random);
	fprintf(fp, PLAYLIST_STATE_FILE_REPEAT "%i\n", playlist->queue.repeat);
	fprintf(fp, PLAYLIST_STATE_FILE_SINGLE "%i\n", playlist->queue.single);
	fprintf(fp, PLAYLIST_STATE_FILE_CONSUME "%i\n",
		playlist->queue.consume);
	fprintf(fp, PLAYLIST_STATE_FILE_CROSSFADE "%i\n",
		(int)(pc_get_cross_fade(pc)));
	fprintf(fp, PLAYLIST_STATE_FILE_MIXRAMPDB "%f\n",
		pc_get_mixramp_db(pc));
	fprintf(fp, PLAYLIST_STATE_FILE_MIXRAMPDELAY "%f\n",
		pc_get_mixramp_delay(pc));
	fputs(PLAYLIST_STATE_FILE_PLAYLIST_BEGIN "\n", fp);
	queue_save(fp, &playlist->queue);
	fputs(PLAYLIST_STATE_FILE_PLAYLIST_END "\n", fp);
}

static void
playlist_state_load(FILE *fp, GString *buffer, struct playlist *playlist)
{
	const char *line = read_text_line(fp, buffer);
	if (line == NULL) {
		g_warning("No playlist in state file");
		return;
	}

	while (!g_str_has_prefix(line, PLAYLIST_STATE_FILE_PLAYLIST_END)) {
		queue_load_song(fp, buffer, line, &playlist->queue);

		line = read_text_line(fp, buffer);
		if (line == NULL) {
			g_warning("'" PLAYLIST_STATE_FILE_PLAYLIST_END
				  "' not found in state file");
			break;
		}
	}

	queue_increment_version(&playlist->queue);
}

bool
playlist_state_restore(const char *line, FILE *fp, GString *buffer,
		       struct playlist *playlist, struct player_control *pc)
{
	int current = -1;
	int seek_time = 0;
	enum player_state state = PLAYER_STATE_STOP;
	bool random_mode = false;

	if (!g_str_has_prefix(line, PLAYLIST_STATE_FILE_STATE))
		return false;

	line += sizeof(PLAYLIST_STATE_FILE_STATE) - 1;

	if (strcmp(line, PLAYLIST_STATE_FILE_STATE_PLAY) == 0)
		state = PLAYER_STATE_PLAY;
	else if (strcmp(line, PLAYLIST_STATE_FILE_STATE_PAUSE) == 0)
		state = PLAYER_STATE_PAUSE;

	while ((line = read_text_line(fp, buffer)) != NULL) {
		if (g_str_has_prefix(line, PLAYLIST_STATE_FILE_TIME)) {
			seek_time =
			    atoi(&(line[strlen(PLAYLIST_STATE_FILE_TIME)]));
		} else if (g_str_has_prefix(line, PLAYLIST_STATE_FILE_REPEAT)) {
			if (strcmp
			    (&(line[strlen(PLAYLIST_STATE_FILE_REPEAT)]),
			     "1") == 0) {
				playlist_set_repeat(playlist, pc, true);
			} else
				playlist_set_repeat(playlist, pc, false);
		} else if (g_str_has_prefix(line, PLAYLIST_STATE_FILE_SINGLE)) {
			if (strcmp
			    (&(line[strlen(PLAYLIST_STATE_FILE_SINGLE)]),
			     "1") == 0) {
				playlist_set_single(playlist, pc, true);
			} else
				playlist_set_single(playlist, pc, false);
		} else if (g_str_has_prefix(line, PLAYLIST_STATE_FILE_CONSUME)) {
			if (strcmp
			    (&(line[strlen(PLAYLIST_STATE_FILE_CONSUME)]),
			     "1") == 0) {
				playlist_set_consume(playlist, true);
			} else
				playlist_set_consume(playlist, false);
		} else if (g_str_has_prefix(line, PLAYLIST_STATE_FILE_CROSSFADE)) {
			pc_set_cross_fade(pc,
					  atoi(line + strlen(PLAYLIST_STATE_FILE_CROSSFADE)));
		} else if (g_str_has_prefix(line, PLAYLIST_STATE_FILE_MIXRAMPDB)) {
			pc_set_mixramp_db(pc,
					  atof(line + strlen(PLAYLIST_STATE_FILE_MIXRAMPDB)));
		} else if (g_str_has_prefix(line, PLAYLIST_STATE_FILE_MIXRAMPDELAY)) {
			pc_set_mixramp_delay(pc,
					     atof(line + strlen(PLAYLIST_STATE_FILE_MIXRAMPDELAY)));
		} else if (g_str_has_prefix(line, PLAYLIST_STATE_FILE_RANDOM)) {
			random_mode =
				strcmp(line + strlen(PLAYLIST_STATE_FILE_RANDOM),
				       "1") == 0;
		} else if (g_str_has_prefix(line, PLAYLIST_STATE_FILE_CURRENT)) {
			current = atoi(&(line
					 [strlen
					  (PLAYLIST_STATE_FILE_CURRENT)]));
		} else if (g_str_has_prefix(line,
					    PLAYLIST_STATE_FILE_PLAYLIST_BEGIN)) {
			playlist_state_load(fp, buffer, playlist);
		}
	}

	playlist_set_random(playlist, pc, random_mode);

	if (!queue_is_empty(&playlist->queue)) {
		if (!queue_valid_position(&playlist->queue, current))
			current = 0;

		if (state == PLAYER_STATE_PLAY &&
		    config_get_bool("restore_paused", false))
			/* the user doesn't want MPD to auto-start
			   playback after startup; fall back to
			   "pause" */
			state = PLAYER_STATE_PAUSE;

		/* enable all devices for the first time; this must be
		   called here, after the audio output states were
		   restored, before playback begins */
		if (state != PLAYER_STATE_STOP)
			pc_update_audio(pc);

		if (state == PLAYER_STATE_STOP /* && config_option */)
			playlist->current = current;
		else if (seek_time == 0)
			playlist_play(playlist, pc, current);
		else
			playlist_seek_song(playlist, pc, current, seek_time);

		if (state == PLAYER_STATE_PAUSE)
			pc_pause(pc);
	}

	return true;
}

unsigned
playlist_state_get_hash(const struct playlist *playlist,
			struct player_control *pc)
{
	struct player_status player_status;

	pc_get_status(pc, &player_status);

	return playlist->queue.version ^
		(player_status.state != PLAYER_STATE_STOP
		 ? ((int)player_status.elapsed_time << 8)
		 : 0) ^
		(playlist->current >= 0
		 ? (queue_order_to_position(&playlist->queue,
					    playlist->current) << 16)
		 : 0) ^
		((int)pc_get_cross_fade(pc) << 20) ^
		(player_status.state << 24) ^
		(playlist->queue.random << 27) ^
		(playlist->queue.repeat << 28) ^
		(playlist->queue.single << 29) ^
		(playlist->queue.consume << 30) ^
		(playlist->queue.random << 31);
}
