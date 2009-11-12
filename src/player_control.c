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

#include "config.h"
#include "player_control.h"
#include "decoder_control.h"
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

	pc.mutex = g_mutex_new();
	pc.cond = g_cond_new();

	pc.command = PLAYER_COMMAND_NONE;
	pc.error = PLAYER_ERROR_NOERROR;
	pc.state = PLAYER_STATE_STOP;
	pc.cross_fade_seconds = 0;
}

void pc_deinit(void)
{
	g_cond_free(pc.cond);
	g_mutex_free(pc.mutex);
}

void
player_wait_decoder(struct decoder_control *dc)
{
	/* during this function, the decoder lock is held, because
	   we're waiting for the decoder thread */
	g_cond_wait(pc.cond, dc->mutex);
}

void
pc_song_deleted(const struct song *song)
{
	if (pc.errored_song == song) {
		pc.error = PLAYER_ERROR_NOERROR;
		pc.errored_song = NULL;
	}
}

static void
player_command_wait_locked(void)
{
	while (pc.command != PLAYER_COMMAND_NONE)
		g_cond_wait(main_cond, pc.mutex);
}

static void
player_command_locked(enum player_command cmd)
{
	assert(pc.command == PLAYER_COMMAND_NONE);

	pc.command = cmd;
	player_signal();
	player_command_wait_locked();
}

static void
player_command(enum player_command cmd)
{
	player_lock();
	player_command_locked(cmd);
	player_unlock();
}

void
pc_play(struct song *song)
{
	assert(song != NULL);

	if (pc.state != PLAYER_STATE_STOP)
		player_command(PLAYER_COMMAND_STOP);

	assert(pc.next_song == NULL);

	pc_enqueue_song(song);

	assert(pc.next_song == NULL);

	idle_add(IDLE_PLAYER);
}

void pc_cancel(void)
{
	player_command(PLAYER_COMMAND_CANCEL);
	assert(pc.next_song == NULL);
}

void
pc_stop(void)
{
	player_command(PLAYER_COMMAND_CLOSE_AUDIO);
	assert(pc.next_song == NULL);

	idle_add(IDLE_PLAYER);
}

void
pc_update_audio(void)
{
	player_command(PLAYER_COMMAND_UPDATE_AUDIO);
}

void
pc_kill(void)
{
	assert(pc.thread != NULL);

	player_command(PLAYER_COMMAND_EXIT);
	g_thread_join(pc.thread);
	pc.thread = NULL;

	idle_add(IDLE_PLAYER);
}

void
pc_pause(void)
{
	if (pc.state != PLAYER_STATE_STOP) {
		player_command(PLAYER_COMMAND_PAUSE);
		idle_add(IDLE_PLAYER);
	}
}

void
pc_set_pause(bool pause_flag)
{
	switch (pc.state) {
	case PLAYER_STATE_STOP:
		break;

	case PLAYER_STATE_PLAY:
		if (pause_flag)
			pc_pause();
		break;

	case PLAYER_STATE_PAUSE:
		if (!pause_flag)
			pc_pause();
		break;
	}
}

void
pc_get_status(struct player_status *status)
{
	player_lock();
	player_command_locked(PLAYER_COMMAND_REFRESH);

	status->state = pc.state;

	if (pc.state != PLAYER_STATE_STOP) {
		status->bit_rate = pc.bit_rate;
		status->audio_format = pc.audio_format;
		status->total_time = pc.total_time;
		status->elapsed_time = pc.elapsed_time;
	}

	player_unlock();
}

enum player_state
pc_get_state(void)
{
	return pc.state;
}

void
pc_clear_error(void)
{
	pc.error = PLAYER_ERROR_NOERROR;
	pc.errored_song = NULL;
}

enum player_error
pc_get_error(void)
{
	return pc.error;
}

static char *
pc_errored_song_uri(void)
{
	return song_get_uri(pc.errored_song);
}

char *
pc_get_error_message(void)
{
	char *error;
	char *uri;

	switch (pc.error) {
	case PLAYER_ERROR_NOERROR:
		return NULL;

	case PLAYER_ERROR_FILENOTFOUND:
		uri = pc_errored_song_uri();
		error = g_strdup_printf("file \"%s\" does not exist or is inaccessible", uri);
		g_free(uri);
		return error;

	case PLAYER_ERROR_FILE:
		uri = pc_errored_song_uri();
		error = g_strdup_printf("problems decoding \"%s\"", uri);
		g_free(uri);
		return error;

	case PLAYER_ERROR_AUDIO:
		return g_strdup("problems opening audio device");

	case PLAYER_ERROR_SYSTEM:
		return g_strdup("system error occured");

	case PLAYER_ERROR_UNKTYPE:
		uri = pc_errored_song_uri();
		error = g_strdup_printf("file type of \"%s\" is unknown", uri);
		g_free(uri);
		return error;
	}

	assert(false);
	return NULL;
}

void
pc_enqueue_song(struct song *song)
{
	assert(song != NULL);

	player_lock();
	assert(pc.next_song == NULL);

	pc.next_song = song;
	player_command_locked(PLAYER_COMMAND_QUEUE);
	player_unlock();
}

bool
pc_seek(struct song *song, float seek_time)
{
	assert(song != NULL);

	if (pc.state == PLAYER_STATE_STOP)
		return false;

	player_lock();
	pc.next_song = song;
	pc.seek_where = seek_time;
	player_command_locked(PLAYER_COMMAND_SEEK);
	player_unlock();

	assert(pc.next_song == NULL);

	idle_add(IDLE_PLAYER);

	return true;
}

float
pc_get_cross_fade(void)
{
	return pc.cross_fade_seconds;
}

void
pc_set_cross_fade(float cross_fade_seconds)
{
	if (cross_fade_seconds < 0)
		cross_fade_seconds = 0;
	pc.cross_fade_seconds = cross_fade_seconds;

	idle_add(IDLE_OPTIONS);
}

double
pc_get_total_play_time(void)
{
	return pc.total_play_time;
}
