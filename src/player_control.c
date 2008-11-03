/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "player_control.h"
#include "path.h"
#include "log.h"
#include "tag.h"
#include "song.h"
#include "idle.h"
#include "os_compat.h"
#include "main_notify.h"

struct player_control pc;

void pc_init(unsigned int buffered_before_play)
{
	pc.buffered_before_play = buffered_before_play;
	notify_init(&pc.notify);
	pc.command = PLAYER_COMMAND_NONE;
	pc.error = PLAYER_ERROR_NOERROR;
	pc.state = PLAYER_STATE_STOP;
	pc.cross_fade_seconds = 0;
	pc.software_volume = 1000;
}

void pc_deinit(void)
{
	notify_deinit(&pc.notify);
}

static void player_command(enum player_command cmd)
{
	pc.command = cmd;
	while (pc.command != PLAYER_COMMAND_NONE) {
		notify_signal(&pc.notify);
		wait_main_task();
	}
}

void
playerPlay(struct song *song)
{
	assert(song != NULL);

	if (pc.state != PLAYER_STATE_STOP)
		player_command(PLAYER_COMMAND_STOP);

	pc.next_song = song;
	player_command(PLAYER_COMMAND_PLAY);

	idle_add(IDLE_PLAYER);
}

void pc_cancel(void)
{
	player_command(PLAYER_COMMAND_CANCEL);
}

void playerWait(void)
{
	player_command(PLAYER_COMMAND_CLOSE_AUDIO);

	idle_add(IDLE_PLAYER);
}

void playerKill(void)
{
	player_command(PLAYER_COMMAND_EXIT);

	idle_add(IDLE_PLAYER);
}

void playerPause(void)
{
	if (pc.state != PLAYER_STATE_STOP) {
		player_command(PLAYER_COMMAND_PAUSE);
		idle_add(IDLE_PLAYER);
	}
}

void playerSetPause(int pause_flag)
{
	switch (pc.state) {
	case PLAYER_STATE_STOP:
		break;

	case PLAYER_STATE_PLAY:
		if (pause_flag)
			playerPause();
		break;
	case PLAYER_STATE_PAUSE:
		if (!pause_flag)
			playerPause();
		break;
	}
}

int getPlayerElapsedTime(void)
{
	return (int)(pc.elapsed_time + 0.5);
}

unsigned long getPlayerBitRate(void)
{
	return pc.bit_rate;
}

int getPlayerTotalTime(void)
{
	return (int)(pc.total_time + 0.5);
}

enum player_state getPlayerState(void)
{
	return pc.state;
}

void clearPlayerError(void)
{
	pc.error = 0;
}

enum player_error getPlayerError(void)
{
	return pc.error;
}

char *getPlayerErrorStr(void)
{
	/* static OK here, only one user in main task */
	static char error[MPD_PATH_MAX + 64]; /* still too much */
	static const size_t errorlen = sizeof(error);
	char path_max_tmp[MPD_PATH_MAX];
	*error = '\0'; /* likely */

	switch (pc.error) {
	case PLAYER_ERROR_NOERROR:
		break;

	case PLAYER_ERROR_FILENOTFOUND:
		snprintf(error, errorlen,
			 "file \"%s\" does not exist or is inaccessible",
			 song_get_url(pc.errored_song, path_max_tmp));
		break;
	case PLAYER_ERROR_FILE:
		snprintf(error, errorlen, "problems decoding \"%s\"",
			 song_get_url(pc.errored_song, path_max_tmp));
		break;
	case PLAYER_ERROR_AUDIO:
		strcpy(error, "problems opening audio device");
		break;
	case PLAYER_ERROR_SYSTEM:
		strcpy(error, "system error occured");
		break;
	case PLAYER_ERROR_UNKTYPE:
		snprintf(error, errorlen, "file type of \"%s\" is unknown",
			 song_get_url(pc.errored_song, path_max_tmp));
	}
	return *error ? error : NULL;
}

void
queueSong(struct song *song)
{
	assert(song != NULL);
	assert(pc.next_song == NULL);

	pc.next_song = song;
	player_command(PLAYER_COMMAND_QUEUE);
}

int
playerSeek(struct song *song, float seek_time)
{
	assert(song != NULL);

	if (pc.state == PLAYER_STATE_STOP)
		return -1;

	pc.next_song = song;

	if (pc.error == PLAYER_ERROR_NOERROR) {
		pc.seek_where = seek_time;
		player_command(PLAYER_COMMAND_SEEK);

		idle_add(IDLE_PLAYER);
	}

	return 0;
}

float getPlayerCrossFade(void)
{
	return pc.cross_fade_seconds;
}

void setPlayerCrossFade(float crossFadeInSeconds)
{
	if (crossFadeInSeconds < 0)
		crossFadeInSeconds = 0;
	pc.cross_fade_seconds = crossFadeInSeconds;

	idle_add(IDLE_OPTIONS);
}

void setPlayerSoftwareVolume(int volume)
{
	volume = (volume > 1000) ? 1000 : (volume < 0 ? 0 : volume);
	pc.software_volume = volume;
}

double getPlayerTotalPlayTime(void)
{
	return pc.total_play_time;
}

/* this actually creates a dupe of the current metadata */
struct song *
playerCurrentDecodeSong(void)
{
	return NULL;
}
