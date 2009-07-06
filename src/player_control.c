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

#include "player_control.h"
#include "path.h"
#include "log.h"
#include "tag.h"
#include "song.h"
#include "idle.h"
#include "pcm_volume.h"
#include "main.h"

#include <assert.h>
#include <stdio.h>

struct player_control pc;

void pc_init(unsigned buffer_chunks, unsigned int buffered_before_play)
{
	pc.buffer_chunks = buffer_chunks;
	pc.buffered_before_play = buffered_before_play;
	notify_init(&pc.notify);
	pc.command = PLAYER_COMMAND_NONE;
	pc.error = PLAYER_ERROR_NOERROR;
	pc.state = PLAYER_STATE_STOP;
	pc.cross_fade_seconds = 0;
}

void pc_deinit(void)
{
	notify_deinit(&pc.notify);
}

void
pc_song_deleted(const struct song *song)
{
	if (pc.errored_song == song) {
		pc.error = PLAYER_ERROR_NOERROR;
		pc.errored_song = NULL;
	}
}

static void player_command(enum player_command cmd)
{
	assert(pc.command == PLAYER_COMMAND_NONE);

	pc.command = cmd;
	while (pc.command != PLAYER_COMMAND_NONE) {
		notify_signal(&pc.notify);
		notify_wait(&main_notify);
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
	assert(pc.thread != NULL);

	player_command(PLAYER_COMMAND_EXIT);
	g_thread_join(pc.thread);
	pc.thread = NULL;

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

static char *
pc_errored_song_uri(void)
{
	return song_get_uri(pc.errored_song);
}

char *getPlayerErrorStr(void)
{
	/* static OK here, only one user in main task */
	static char error[MPD_PATH_MAX + 64]; /* still too much */
	static const size_t errorlen = sizeof(error);
	char *uri;

	*error = '\0'; /* likely */

	switch (pc.error) {
	case PLAYER_ERROR_NOERROR:
		break;

	case PLAYER_ERROR_FILENOTFOUND:
		uri = pc_errored_song_uri();
		snprintf(error, errorlen,
			 "file \"%s\" does not exist or is inaccessible", uri);
		g_free(uri);
		break;

	case PLAYER_ERROR_FILE:
		uri = pc_errored_song_uri();
		snprintf(error, errorlen, "problems decoding \"%s\"", uri);
		g_free(uri);
		break;

	case PLAYER_ERROR_AUDIO:
		strcpy(error, "problems opening audio device");
		break;

	case PLAYER_ERROR_SYSTEM:
		strcpy(error, "system error occured");
		break;

	case PLAYER_ERROR_UNKTYPE:
		uri = pc_errored_song_uri();
		snprintf(error, errorlen,
			 "file type of \"%s\" is unknown", uri);
		g_free(uri);
		break;
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

bool
pc_seek(struct song *song, float seek_time)
{
	assert(song != NULL);

	if (pc.state == PLAYER_STATE_STOP)
		return false;

	pc.next_song = song;
	pc.seek_where = seek_time;
	player_command(PLAYER_COMMAND_SEEK);

	assert(pc.next_song == NULL);

	idle_add(IDLE_PLAYER);

	return true;
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

double getPlayerTotalPlayTime(void)
{
	return pc.total_play_time;
}
