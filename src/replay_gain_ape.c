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

#include "config.h"
#include "replay_gain_ape.h"
#include "replay_gain_info.h"
#include "ape.h"

#include <glib.h>

#include <string.h>
#include <stdlib.h>

struct rg_ape_ctx {
	struct replay_gain_info *info;
	bool found;
};

static bool
replay_gain_ape_callback(unsigned long flags, const char *key,
			 const char *_value, size_t value_length, void *_ctx)
{
	struct rg_ape_ctx *ctx = _ctx;

	/* we only care about utf-8 text tags */
	if ((flags & (0x3 << 1)) != 0)
		return true;

	char value[16];
	if (value_length >= sizeof(value))
		return true;
	memcpy(value, _value, value_length);
	value[value_length] = 0;

	if (g_ascii_strcasecmp(key, "replaygain_track_gain") == 0) {
		ctx->info->tuples[REPLAY_GAIN_TRACK].gain = atof(value);
		ctx->found = true;
	} else if (g_ascii_strcasecmp(key, "replaygain_album_gain") == 0) {
		ctx->info->tuples[REPLAY_GAIN_ALBUM].gain = atof(value);
		ctx->found = true;
	} else if (g_ascii_strcasecmp(key, "replaygain_track_peak") == 0) {
		ctx->info->tuples[REPLAY_GAIN_TRACK].peak = atof(value);
		ctx->found = true;
	} else if (g_ascii_strcasecmp(key, "replaygain_album_peak") == 0) {
		ctx->info->tuples[REPLAY_GAIN_ALBUM].peak = atof(value);
		ctx->found = true;
	}

	return true;
}

bool
replay_gain_ape_read(const char *path_fs, struct replay_gain_info *info)
{
	struct rg_ape_ctx ctx = {
		.info = info,
		.found = false,
	};

	return tag_ape_scan(path_fs, replay_gain_ape_callback, &ctx) &&
		ctx.found;
}
