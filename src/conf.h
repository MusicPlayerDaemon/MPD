/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#ifndef MPD_CONF_H
#define MPD_CONF_H

#include <stdbool.h>
#include <glib.h>

#define CONF_MUSIC_DIR                  "music_directory"
#define CONF_PLAYLIST_DIR               "playlist_directory"
#define CONF_FOLLOW_INSIDE_SYMLINKS     "follow_inside_symlinks"
#define CONF_FOLLOW_OUTSIDE_SYMLINKS    "follow_outside_symlinks"
#define CONF_DB_FILE                    "db_file"
#define CONF_STICKER_FILE "sticker_file"
#define CONF_LOG_FILE                   "log_file"
#define CONF_PID_FILE                   "pid_file"
#define CONF_STATE_FILE                 "state_file"
#define CONF_USER                       "user"
#define CONF_GROUP                      "group"
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
#define CONF_REPLAYGAIN                 "replaygain"
#define CONF_REPLAYGAIN_PREAMP          "replaygain_preamp"
#define CONF_REPLAYGAIN_MISSING_PREAMP  "replaygain_missing_preamp"
#define CONF_VOLUME_NORMALIZATION       "volume_normalization"
#define CONF_SAMPLERATE_CONVERTER       "samplerate_converter"
#define CONF_AUDIO_BUFFER_SIZE          "audio_buffer_size"
#define CONF_BUFFER_BEFORE_PLAY         "buffer_before_play"
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
#define CONF_DECODER "decoder"
#define CONF_INPUT "input"
#define CONF_GAPLESS_MP3_PLAYBACK	"gapless_mp3_playback"
#define CONF_PLAYLIST_PLUGIN "playlist_plugin"
#define CONF_AUTO_UPDATE		"auto_update"

#define DEFAULT_PLAYLIST_MAX_LENGTH (1024*16)
#define DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS false

struct block_param {
	char *name;
	char *value;
	int line;

	/**
	 * This flag is false when nobody has queried the value of
	 * this option yet.
	 */
	bool used;
};

struct config_param {
	char *value;
	unsigned int line;

	struct block_param *block_params;
	unsigned num_block_params;

	/**
	 * This flag is false when nobody has queried the value of
	 * this option yet.
	 */
	bool used;
};

/**
 * A GQuark for GError instances, resulting from malformed
 * configuration.
 */
static inline GQuark
config_quark(void)
{
	return g_quark_from_static_string("config");
}

void config_global_init(void);
void config_global_finish(void);

/**
 * Call this function after all configuration has been evaluated.  It
 * checks for unused parameters, and logs warnings.
 */
void config_global_check(void);

bool
config_read_file(const char *file, GError **error_r);

/* don't free the returned value
   set _last_ to NULL to get first entry */
G_GNUC_PURE
struct config_param *
config_get_next_param(const char *name, const struct config_param *last);

G_GNUC_PURE
static inline struct config_param *
config_get_param(const char *name)
{
	return config_get_next_param(name, NULL);
}

/* Note on G_GNUC_PURE: Some of the functions declared pure are not
   really pure in strict sense.  They have side effect such that they
   validate parameter's value and signal an error if it's invalid.
   However, if the argument was already validated or we don't care
   about the argument at all, this may be ignored so in the end, we
   should be fine with calling those functions pure.  */

G_GNUC_PURE
const char *
config_get_string(const char *name, const char *default_value);

/**
 * Returns an optional configuration variable which contains an
 * absolute path.  If there is a tilde prefix, it is expanded.  Aborts
 * MPD if the path is not a valid absolute path.
 */
/* We lie here really.  This function is not pure as it has side
   effects -- it parse the value and creates new string freeing
   previous one.  However, because this works the very same way each
   time (ie. from the outside it appears as if function had no side
   effects) we should be in the clear declaring it pure. */
G_GNUC_PURE
const char *
config_get_path(const char *name);

G_GNUC_PURE
unsigned
config_get_positive(const char *name, unsigned default_value);

G_GNUC_PURE
struct block_param *
config_get_block_param(const struct config_param *param, const char *name);

G_GNUC_PURE
bool config_get_bool(const char *name, bool default_value);

G_GNUC_PURE
const char *
config_get_block_string(const struct config_param *param, const char *name,
			const char *default_value);

static inline char *
config_dup_block_string(const struct config_param *param, const char *name,
			const char *default_value)
{
	return g_strdup(config_get_block_string(param, name, default_value));
}

G_GNUC_PURE
unsigned
config_get_block_unsigned(const struct config_param *param, const char *name,
			  unsigned default_value);

G_GNUC_PURE
bool
config_get_block_bool(const struct config_param *param, const char *name,
		      bool default_value);

struct config_param *
config_new_param(const char *value, int line);

bool
config_add_block_param(struct config_param * param, const char *name,
		       const char *value, int line, GError **error_r);

#endif
