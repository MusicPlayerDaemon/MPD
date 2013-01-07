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
#include "PlayerControl.hxx"

extern "C" {
#include "idle.h"
}

#include "song.h"
#include "DecoderControl.hxx"
#include "Main.hxx"

#include <cmath>

#include <assert.h>
#include <stdio.h>

static void
pc_enqueue_song_locked(struct player_control *pc, struct song *song);

player_control::player_control(unsigned _buffer_chunks,
			       unsigned _buffered_before_play)
	:buffer_chunks(_buffer_chunks),
	 buffered_before_play(_buffered_before_play),
	 thread(nullptr),
	 mutex(g_mutex_new()),
	 cond(g_cond_new()),
	 command(PLAYER_COMMAND_NONE),
	 state(PLAYER_STATE_STOP),
	 error_type(PLAYER_ERROR_NONE),
	 error(nullptr),
	 next_song(nullptr),
	 cross_fade_seconds(0),
	 mixramp_db(0),
	 mixramp_delay_seconds(std::nanf("")),
	 total_play_time(0)
{
}

player_control::~player_control()
{
	if (next_song != nullptr)
		song_free(next_song);

	g_cond_free(cond);
	g_mutex_free(mutex);
}

void
player_wait_decoder(struct player_control *pc, struct decoder_control *dc)
{
	assert(pc != NULL);
	assert(dc != NULL);
	assert(dc->client_cond == pc->cond);

	/* during this function, the decoder lock is held, because
	   we're waiting for the decoder thread */
	g_cond_wait(pc->cond, dc->mutex);
}

static void
player_command_wait_locked(struct player_control *pc)
{
	while (pc->command != PLAYER_COMMAND_NONE)
		g_cond_wait(main_cond, pc->mutex);
}

static void
player_command_locked(struct player_control *pc, enum player_command cmd)
{
	assert(pc->command == PLAYER_COMMAND_NONE);

	pc->command = cmd;
	player_signal(pc);
	player_command_wait_locked(pc);
}

static void
player_command(struct player_control *pc, enum player_command cmd)
{
	player_lock(pc);
	player_command_locked(pc, cmd);
	player_unlock(pc);
}

void
pc_play(struct player_control *pc, struct song *song)
{
	assert(song != NULL);

	player_lock(pc);

	if (pc->state != PLAYER_STATE_STOP)
		player_command_locked(pc, PLAYER_COMMAND_STOP);

	assert(pc->next_song == NULL);

	pc_enqueue_song_locked(pc, song);

	assert(pc->next_song == NULL);

	player_unlock(pc);

	idle_add(IDLE_PLAYER);
}

void
pc_cancel(struct player_control *pc)
{
	player_command(pc, PLAYER_COMMAND_CANCEL);
	assert(pc->next_song == NULL);
}

void
pc_stop(struct player_control *pc)
{
	player_command(pc, PLAYER_COMMAND_CLOSE_AUDIO);
	assert(pc->next_song == NULL);

	idle_add(IDLE_PLAYER);
}

void
pc_update_audio(struct player_control *pc)
{
	player_command(pc, PLAYER_COMMAND_UPDATE_AUDIO);
}

void
pc_kill(struct player_control *pc)
{
	assert(pc->thread != NULL);

	player_command(pc, PLAYER_COMMAND_EXIT);
	g_thread_join(pc->thread);
	pc->thread = NULL;

	idle_add(IDLE_PLAYER);
}

void
pc_pause(struct player_control *pc)
{
	player_lock(pc);

	if (pc->state != PLAYER_STATE_STOP) {
		player_command_locked(pc, PLAYER_COMMAND_PAUSE);
		idle_add(IDLE_PLAYER);
	}

	player_unlock(pc);
}

static void
pc_pause_locked(struct player_control *pc)
{
	if (pc->state != PLAYER_STATE_STOP) {
		player_command_locked(pc, PLAYER_COMMAND_PAUSE);
		idle_add(IDLE_PLAYER);
	}
}

void
pc_set_pause(struct player_control *pc, bool pause_flag)
{
	player_lock(pc);

	switch (pc->state) {
	case PLAYER_STATE_STOP:
		break;

	case PLAYER_STATE_PLAY:
		if (pause_flag)
			pc_pause_locked(pc);
		break;

	case PLAYER_STATE_PAUSE:
		if (!pause_flag)
			pc_pause_locked(pc);
		break;
	}

	player_unlock(pc);
}

void
pc_set_border_pause(struct player_control *pc, bool border_pause)
{
	player_lock(pc);
	pc->border_pause = border_pause;
	player_unlock(pc);
}

void
pc_get_status(struct player_control *pc, struct player_status *status)
{
	player_lock(pc);
	player_command_locked(pc, PLAYER_COMMAND_REFRESH);

	status->state = pc->state;

	if (pc->state != PLAYER_STATE_STOP) {
		status->bit_rate = pc->bit_rate;
		status->audio_format = pc->audio_format;
		status->total_time = pc->total_time;
		status->elapsed_time = pc->elapsed_time;
	}

	player_unlock(pc);
}

void
pc_set_error(struct player_control *pc, enum player_error type,
	     GError *error)
{
	assert(pc != NULL);
	assert(type != PLAYER_ERROR_NONE);
	assert(error != NULL);

	if (pc->error_type != PLAYER_ERROR_NONE)
	    g_error_free(pc->error);

	pc->error_type = type;
	pc->error = error;
}

void
pc_clear_error(struct player_control *pc)
{
	player_lock(pc);

	if (pc->error_type != PLAYER_ERROR_NONE) {
	    pc->error_type = PLAYER_ERROR_NONE;
	    g_error_free(pc->error);
	}

	player_unlock(pc);
}

char *
pc_get_error_message(struct player_control *pc)
{
	player_lock(pc);
	char *message = pc->error_type != PLAYER_ERROR_NONE
		? g_strdup(pc->error->message)
		: NULL;
	player_unlock(pc);
	return message;
}

static void
pc_enqueue_song_locked(struct player_control *pc, struct song *song)
{
	assert(song != NULL);
	assert(pc->next_song == NULL);

	pc->next_song = song;
	player_command_locked(pc, PLAYER_COMMAND_QUEUE);
}

void
pc_enqueue_song(struct player_control *pc, struct song *song)
{
	assert(song != NULL);

	player_lock(pc);
	pc_enqueue_song_locked(pc, song);
	player_unlock(pc);
}

bool
pc_seek(struct player_control *pc, struct song *song, float seek_time)
{
	assert(song != NULL);

	player_lock(pc);

	if (pc->next_song != NULL)
		song_free(pc->next_song);

	pc->next_song = song;
	pc->seek_where = seek_time;
	player_command_locked(pc, PLAYER_COMMAND_SEEK);
	player_unlock(pc);

	assert(pc->next_song == NULL);

	idle_add(IDLE_PLAYER);

	return true;
}

float
pc_get_cross_fade(const struct player_control *pc)
{
	return pc->cross_fade_seconds;
}

void
pc_set_cross_fade(struct player_control *pc, float cross_fade_seconds)
{
	if (cross_fade_seconds < 0)
		cross_fade_seconds = 0;
	pc->cross_fade_seconds = cross_fade_seconds;

	idle_add(IDLE_OPTIONS);
}

void
pc_set_mixramp_db(struct player_control *pc, float mixramp_db)
{
	pc->mixramp_db = mixramp_db;

	idle_add(IDLE_OPTIONS);
}

void
pc_set_mixramp_delay(struct player_control *pc, float mixramp_delay_seconds)
{
	pc->mixramp_delay_seconds = mixramp_delay_seconds;

	idle_add(IDLE_OPTIONS);
}
