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
#include "encoder_list.h"
#include "encoder_plugin.h"

#include <string.h>

extern const struct encoder_plugin null_encoder_plugin;
extern const struct encoder_plugin vorbis_encoder_plugin;
extern const struct encoder_plugin lame_encoder_plugin;
extern const struct encoder_plugin twolame_encoder_plugin;
extern const struct encoder_plugin wave_encoder_plugin;
extern const struct encoder_plugin flac_encoder_plugin;

const struct encoder_plugin *const encoder_plugins[] = {
	&null_encoder_plugin,
#ifdef ENABLE_VORBIS_ENCODER
	&vorbis_encoder_plugin,
#endif
#ifdef ENABLE_LAME_ENCODER
	&lame_encoder_plugin,
#endif
#ifdef ENABLE_TWOLAME_ENCODER
	&twolame_encoder_plugin,
#endif
#ifdef ENABLE_WAVE_ENCODER
	&wave_encoder_plugin,
#endif
#ifdef ENABLE_FLAC_ENCODER
	&flac_encoder_plugin,
#endif
	NULL
};

const struct encoder_plugin *
encoder_plugin_get(const char *name)
{
	encoder_plugins_for_each(plugin)
		if (strcmp(plugin->name, name) == 0)
			return plugin;

	return NULL;
}
