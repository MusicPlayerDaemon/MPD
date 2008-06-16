/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CONF_H
#define CONF_H

#include "../config.h"

#define CONF_MUSIC_DIR                  "music_directory"
#define CONF_PLAYLIST_DIR               "playlist_directory"
#define CONF_DB_FILE                    "db_file"
#define CONF_LOG_FILE                   "log_file"
#define CONF_ERROR_FILE                 "error_file"
#define CONF_PID_FILE                   "pid_file"
#define CONF_STATE_FILE                 "state_file"
#define CONF_USER                       "user"
#define CONF_BIND_TO_ADDRESS            "bind_to_address"
#define CONF_PORT                       "port"
#define CONF_LOG_LEVEL                  "log_level"
#define CONF_ZEROCONF_NAME              "zeroconf_name"
#define CONF_ZEROCONF_ENABLED			"zeroconf_enabled"
#define CONF_PASSWORD                   "password"
#define CONF_DEFAULT_PERMS              "default_permissions"
#define CONF_AUDIO_OUTPUT               "audio_output"
#define CONF_AUDIO_OUTPUT_FORMAT        "audio_output_format"
#define CONF_MIXER_TYPE                 "mixer_type"
#define CONF_MIXER_DEVICE               "mixer_device"
#define CONF_MIXER_CONTROL              "mixer_control"
#define CONF_REPLAYGAIN                 "replaygain"
#define CONF_REPLAYGAIN_PREAMP          "replaygain_preamp"
#define CONF_VOLUME_NORMALIZATION       "volume_normalization"
#define CONF_SAMPLERATE_CONVERTER       "samplerate_converter"
#define CONF_AUDIO_BUFFER_SIZE          "audio_buffer_size"
#define CONF_BUFFER_BEFORE_PLAY         "buffer_before_play"
#define CONF_HTTP_BUFFER_SIZE           "http_buffer_size"
#define CONF_HTTP_PREBUFFER_SIZE        "http_prebuffer_size"
#define CONF_HTTP_PROXY_HOST            "http_proxy_host"
#define CONF_HTTP_PROXY_PORT            "http_proxy_port"
#define CONF_HTTP_PROXY_USER            "http_proxy_user"
#define CONF_HTTP_PROXY_PASSWORD        "http_proxy_password"
#define CONF_CONN_TIMEOUT               "connection_timeout"
#define CONF_MAX_CONN                   "max_connections"
#define CONF_MAX_PLAYLIST_LENGTH        "max_playlist_length"
#define CONF_MAX_COMMAND_LIST_SIZE      "max_command_list_size"
#define CONF_MAX_OUTPUT_BUFFER_SIZE     "max_output_buffer_size"
#define CONF_FS_CHARSET                 "filesystem_charset"
#define CONF_ID3V1_ENCODING             "id3v1_encoding"
#define CONF_METADATA_TO_USE            "metadata_to_use"
#define CONF_SAVE_ABSOLUTE_PATHS        "save_absolute_paths_in_playlists"
#define CONF_GAPLESS_MP3_PLAYBACK	"gapless_mp3_playback"

typedef struct _BlockParam {
	char *name;
	char *value;
	int line;
} BlockParam;

typedef struct _ConfigParam {
	char *value;
	unsigned int line;
	BlockParam *blockParams;
	int numberOfBlockParams;
} ConfigParam;

void initConf(void);
void finishConf(void);

void readConf(char *file);

/* don't free the returned value
   set _last_ to NULL to get first entry */
ConfigParam *getNextConfigParam(char *name, ConfigParam * last);

#define getConfigParam(name) 	getNextConfigParam(name, NULL)

char *getConfigParamValue(char *name);

int getBoolConfigParam(char *name);

BlockParam *getBlockParam(ConfigParam * param, char *name);

ConfigParam *parseConfigFilePath(char *name, int force);

#endif
