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
#include "replay_gain_info.h"

#include <glib.h>

struct replay_gain_info *
replay_gain_info_new(void)
{
	struct replay_gain_info *ret = g_new(struct replay_gain_info, 1);

	for (unsigned i = 0; i < G_N_ELEMENTS(ret->tuples); ++i) {
		ret->tuples[i].gain = INFINITY;
		ret->tuples[i].peak = 0.0;
	}

	return ret;
}

struct replay_gain_info *
replay_gain_info_dup(const struct replay_gain_info *src)
{
	return g_memdup(src, sizeof(*src));
}

void
replay_gain_info_free(struct replay_gain_info *info)
{
	g_free(info);
}

float
replay_gain_tuple_scale(const struct replay_gain_tuple *tuple, float preamp)
{
	float scale;

	scale = pow(10.0, tuple->gain / 20.0);
	scale *= preamp;
	if (scale > 15.0)
		scale = 15.0;

	if (scale * tuple->peak > 1.0)
		scale = 1.0 / tuple->peak;

	return scale;
}
