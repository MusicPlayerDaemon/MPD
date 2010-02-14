/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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
#include "replay_gain_state.h"
#include "pcm_volume.h"

#include <glib.h>

#include <assert.h>

struct replay_gain_state {
	float preamp, missing_preamp;

	enum replay_gain_mode mode;

	struct replay_gain_info info;

	float scale;
};

struct replay_gain_state *
replay_gain_state_new(float preamp, float missing_preamp)
{
	struct replay_gain_state *state = g_new(struct replay_gain_state, 1);

	state->preamp = preamp;
	state->scale = state->missing_preamp = missing_preamp;
	state->mode = REPLAY_GAIN_OFF;
	replay_gain_info_init(&state->info);

	return state;
}

void
replay_gain_state_free(struct replay_gain_state *state)
{
	assert(state != NULL);

	g_free(state);
}

static void
replay_gain_state_calc_scale(struct replay_gain_state *state)
{
	assert(state != NULL);

	if (state->mode == REPLAY_GAIN_OFF)
		return;

	const struct replay_gain_tuple *tuple =
		&state->info.tuples[state->mode];
	if (replay_gain_tuple_defined(tuple)) {
		g_debug("computing ReplayGain scale with gain %f, peak %f",
			tuple->gain, tuple->peak);

		state->scale = replay_gain_tuple_scale(tuple, state->preamp);
	} else
		state->scale = state->missing_preamp;
}

void
replay_gain_state_set_mode(struct replay_gain_state *state,
			   enum replay_gain_mode mode)
{
	assert(state != NULL);

	if (mode == state->mode)
		return;

	state->mode = mode;

	replay_gain_state_calc_scale(state);
}

void
replay_gain_state_set_info(struct replay_gain_state *state,
			   const struct replay_gain_info *info)
{
	assert(state != NULL);

	if (info != NULL)
		state->info = *info;
	else
		replay_gain_info_init(&state->info);

	replay_gain_state_calc_scale(state);
}

void
replay_gain_state_apply(const struct replay_gain_state *state,
			void *buffer, size_t size,
			const struct audio_format *format)
{
	assert(state != NULL);

	if (state->mode == REPLAY_GAIN_OFF)
		return;

	pcm_volume(buffer, size, format, pcm_float_to_volume(state->scale));
}
