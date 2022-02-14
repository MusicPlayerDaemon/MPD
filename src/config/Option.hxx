/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "util/Compiler.h"

#if defined(_WIN32) && CLANG_OR_GCC_VERSION(4,7)
/* "INPUT" is declared by winuser.h */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif

enum class ConfigOption {
	MUSIC_DIR,
	PLAYLIST_DIR,
	FOLLOW_INSIDE_SYMLINKS,
	FOLLOW_OUTSIDE_SYMLINKS,
	DB_FILE,
	STICKER_FILE,
	LOG_FILE,
	PID_FILE,
	STATE_FILE,
	STATE_FILE_INTERVAL,
	RESTORE_PAUSED,
	USER,
	GROUP,
	BIND_TO_ADDRESS,
	PORT,
	LOG_LEVEL,
	ZEROCONF_NAME,
	ZEROCONF_ENABLED,
	PASSWORD,
	HOST_PERMISSIONS,
	LOCAL_PERMISSIONS,
	DEFAULT_PERMS,
	AUDIO_OUTPUT_FORMAT,
	MIXER_TYPE,
	REPLAYGAIN,
	REPLAYGAIN_PREAMP,
	REPLAYGAIN_MISSING_PREAMP,
	REPLAYGAIN_LIMIT,
	VOLUME_NORMALIZATION,
	SAMPLERATE_CONVERTER,
	AUDIO_BUFFER_SIZE,
	BUFFER_BEFORE_PLAY,
	HTTP_PROXY_HOST,
	HTTP_PROXY_PORT,
	HTTP_PROXY_USER,
	HTTP_PROXY_PASSWORD,
	CONN_TIMEOUT,
	MAX_CONN,
	MAX_PLAYLIST_LENGTH,
	MAX_COMMAND_LIST_SIZE,
	MAX_OUTPUT_BUFFER_SIZE,
	FS_CHARSET,
	ID3V1_ENCODING,
	METADATA_TO_USE,
	SAVE_ABSOLUTE_PATHS,
	GAPLESS_MP3_PLAYBACK,
	AUTO_UPDATE,
	AUTO_UPDATE_DEPTH,
	DESPOTIFY_USER,
	DESPOTIFY_PASSWORD,
	DESPOTIFY_HIGH_BITRATE,

	MIXRAMP_ANALYZER,

	MAX
};

enum class ConfigBlockOption {
	AUDIO_OUTPUT,
	DECODER,
	INPUT,
	INPUT_CACHE,
	ARCHIVE_PLUGIN,
	PLAYLIST_PLUGIN,
	RESAMPLER,
	AUDIO_FILTER,
	DATABASE,
	NEIGHBORS,
	MAX
};

#if defined(_WIN32) && CLANG_OR_GCC_VERSION(4,7)
#pragma GCC diagnostic pop
#endif

/**
 * @return #ConfigOption::MAX if not found
 */
[[gnu::pure]]
enum ConfigOption
ParseConfigOptionName(const char *name) noexcept;

/**
 * @return #ConfigOption::MAX if not found
 */
[[gnu::pure]]
enum ConfigBlockOption
ParseConfigBlockOptionName(const char *name) noexcept;

#endif
