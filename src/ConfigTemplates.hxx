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

#include "gcc.h"

#include <glib.h>

#include <string.h>

struct ConfigTemplate {
	const char *const name;
	const bool repeatable;
	const bool block;
};

static constexpr struct ConfigTemplate config_templates[] = {
	{ CONF_MUSIC_DIR, false, false },
	{ CONF_PLAYLIST_DIR, false, false },
	{ CONF_FOLLOW_INSIDE_SYMLINKS, false, false },
	{ CONF_FOLLOW_OUTSIDE_SYMLINKS, false, false },
	{ CONF_DB_FILE, false, false },
	{ CONF_STICKER_FILE, false, false },
	{ CONF_LOG_FILE, false, false },
	{ CONF_PID_FILE, false, false },
	{ CONF_STATE_FILE, false, false },
	{ "restore_paused", false, false },
	{ CONF_USER, false, false },
	{ CONF_GROUP, false, false },
	{ CONF_BIND_TO_ADDRESS, true, false },
	{ CONF_PORT, false, false },
	{ CONF_LOG_LEVEL, false, false },
	{ CONF_ZEROCONF_NAME, false, false },
	{ CONF_ZEROCONF_ENABLED, false, false },
	{ CONF_PASSWORD, true, false },
	{ CONF_DEFAULT_PERMS, false, false },
	{ CONF_AUDIO_OUTPUT, true, true },
	{ CONF_AUDIO_OUTPUT_FORMAT, false, false },
	{ CONF_MIXER_TYPE, false, false },
	{ CONF_REPLAYGAIN, false, false },
	{ CONF_REPLAYGAIN_PREAMP, false, false },
	{ CONF_REPLAYGAIN_MISSING_PREAMP, false, false },
	{ CONF_REPLAYGAIN_LIMIT, false, false },
	{ CONF_VOLUME_NORMALIZATION, false, false },
	{ CONF_SAMPLERATE_CONVERTER, false, false },
	{ CONF_AUDIO_BUFFER_SIZE, false, false },
	{ CONF_BUFFER_BEFORE_PLAY, false, false },
	{ CONF_HTTP_PROXY_HOST, false, false },
	{ CONF_HTTP_PROXY_PORT, false, false },
	{ CONF_HTTP_PROXY_USER, false, false },
	{ CONF_HTTP_PROXY_PASSWORD, false, false },
	{ CONF_CONN_TIMEOUT, false, false },
	{ CONF_MAX_CONN, false, false },
	{ CONF_MAX_PLAYLIST_LENGTH, false, false },
	{ CONF_MAX_COMMAND_LIST_SIZE, false, false },
	{ CONF_MAX_OUTPUT_BUFFER_SIZE, false, false },
	{ CONF_FS_CHARSET, false, false },
	{ CONF_ID3V1_ENCODING, false, false },
	{ CONF_METADATA_TO_USE, false, false },
	{ CONF_SAVE_ABSOLUTE_PATHS, false, false },
	{ CONF_DECODER, true, true },
	{ CONF_INPUT, true, true },
	{ CONF_GAPLESS_MP3_PLAYBACK, false, false },
	{ CONF_PLAYLIST_PLUGIN, true, true },
	{ CONF_AUTO_UPDATE, false, false },
	{ CONF_AUTO_UPDATE_DEPTH, false, false },
	{ CONF_DESPOTIFY_USER, false, false },
	{ CONF_DESPOTIFY_PASSWORD, false, false},
	{ CONF_DESPOTIFY_HIGH_BITRATE, false, false },
	{ "filter", true, true },
	{ "database", false, true },
};

gcc_pure
static int
ConfigFindByName(const char *name)
{
	for (unsigned i = 0; i < G_N_ELEMENTS(config_templates); ++i)
		if (strcmp(config_templates[i].name, name) == 0)
			return i;

	return -1;
}
