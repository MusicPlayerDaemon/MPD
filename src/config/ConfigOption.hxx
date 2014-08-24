/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_CONFIG_OPTION_HXX
#define MPD_CONFIG_OPTION_HXX

#include "Compiler.h"

enum ConfigOption {
	CONF_MUSIC_DIR,
	CONF_PLAYLIST_DIR,
	CONF_FOLLOW_INSIDE_SYMLINKS,
	CONF_FOLLOW_OUTSIDE_SYMLINKS,
	CONF_DB_FILE,
	CONF_STICKER_FILE,
	CONF_LOG_FILE,
	CONF_PID_FILE,
	CONF_STATE_FILE,
	CONF_STATE_FILE_INTERVAL,
	CONF_RESTORE_PAUSED,
	CONF_USER,
	CONF_GROUP,
	CONF_BIND_TO_ADDRESS,
	CONF_PORT,
	CONF_LOG_LEVEL,
	CONF_ZEROCONF_NAME,
	CONF_ZEROCONF_ENABLED,
	CONF_PASSWORD,
	CONF_DEFAULT_PERMS,
	CONF_AUDIO_OUTPUT,
	CONF_AUDIO_OUTPUT_FORMAT,
	CONF_MIXER_TYPE,
	CONF_REPLAYGAIN,
	CONF_REPLAYGAIN_PREAMP,
	CONF_REPLAYGAIN_MISSING_PREAMP,
	CONF_REPLAYGAIN_LIMIT,
	CONF_VOLUME_NORMALIZATION,
	CONF_SAMPLERATE_CONVERTER,
	CONF_AUDIO_BUFFER_SIZE,
	CONF_BUFFER_BEFORE_PLAY,
	CONF_HTTP_PROXY_HOST,
	CONF_HTTP_PROXY_PORT,
	CONF_HTTP_PROXY_USER,
	CONF_HTTP_PROXY_PASSWORD,
	CONF_CONN_TIMEOUT,
	CONF_MAX_CONN,
	CONF_MAX_PLAYLIST_LENGTH,
	CONF_MAX_COMMAND_LIST_SIZE,
	CONF_MAX_OUTPUT_BUFFER_SIZE,
	CONF_FS_CHARSET,
	CONF_ID3V1_ENCODING,
	CONF_METADATA_TO_USE,
	CONF_SAVE_ABSOLUTE_PATHS,
	CONF_DECODER,
	CONF_INPUT,
	CONF_GAPLESS_MP3_PLAYBACK,
	CONF_PLAYLIST_PLUGIN,
	CONF_AUTO_UPDATE,
	CONF_AUTO_UPDATE_DEPTH,
	CONF_DESPOTIFY_USER,
	CONF_DESPOTIFY_PASSWORD,
	CONF_DESPOTIFY_HIGH_BITRATE,
	CONF_AUDIO_FILTER,
	CONF_DATABASE,
	CONF_NEIGHBORS,
	CONF_MAX
};

/**
 * @return #CONF_MAX if not found
 */
gcc_pure
enum ConfigOption
ParseConfigOptionName(const char *name);

#endif
