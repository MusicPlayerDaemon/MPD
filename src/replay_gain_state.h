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

#ifndef MPD_REPLAY_GAIN_STATE_H
#define MPD_REPLAY_GAIN_STATE_H

#include "check.h"
#include "replay_gain_info.h"

#include <stddef.h>

struct replay_gain_state;
struct audio_format;

struct replay_gain_state *
replay_gain_state_new(float preamp, float missing_preamp);

void
replay_gain_state_free(struct replay_gain_state *state);

void
replay_gain_state_set_mode(struct replay_gain_state *state,
			   enum replay_gain_mode mode);

void
replay_gain_state_set_info(struct replay_gain_state *state,
			   const struct replay_gain_info *info);

void
replay_gain_state_apply(const struct replay_gain_state *state,
			void *buffer, size_t size,
			const struct audio_format *format);

#endif
