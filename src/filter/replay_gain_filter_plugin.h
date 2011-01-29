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

#ifndef REPLAY_GAIN_FILTER_PLUGIN_H
#define REPLAY_GAIN_FILTER_PLUGIN_H

#include "replay_gain_info.h"

struct filter;
struct mixer;

/**
 * Enables or disables the hardware mixer for applying replay gain.
 *
 * @param mixer the hardware mixer, or NULL to fall back to software
 * volume
 * @param base the base volume level for scale=1.0, between 1 and 100
 * (including).
 */
void
replay_gain_filter_set_mixer(struct filter *_filter, struct mixer *mixer,
			     unsigned base);

/**
 * Sets a new #replay_gain_info at the beginning of a new song.
 *
 * @param info the new #replay_gain_info value, or NULL if no replay
 * gain data is available for the current song
 */
void
replay_gain_filter_set_info(struct filter *filter,
			    const struct replay_gain_info *info);

#endif
