/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#include "ConfigTemplates.hxx"
#include "ConfigOption.hxx"
#include "util/Macros.hxx"

#include <string.h>

const ConfigTemplate config_param_templates[] = {
	{ "music_directory", false },
	{ "playlist_directory", false },
	{ "follow_inside_symlinks", false },
	{ "follow_outside_symlinks", false },
	{ "db_file", false },
	{ "sticker_file", false },
	{ "log_file", false },
	{ "pid_file", false },
	{ "state_file", false },
	{ "state_file_interval", false },
	{ "restore_paused", false },
	{ "user", false },
	{ "group", false },
	{ "bind_to_address", true },
	{ "port", false },
	{ "log_level", false },
	{ "zeroconf_name", false },
	{ "zeroconf_enabled", false },
	{ "password", true },
	{ "default_permissions", false },
	{ "audio_output_format", false },
	{ "mixer_type", false },
	{ "replaygain", false },
	{ "replaygain_preamp", false },
	{ "replaygain_missing_preamp", false },
	{ "replaygain_limit", false },
	{ "volume_normalization", false },
	{ "samplerate_converter", false },
	{ "audio_buffer_size", false },
	{ "buffer_before_play", false },
	{ "http_proxy_host", false },
	{ "http_proxy_port", false },
	{ "http_proxy_user", false },
	{ "http_proxy_password", false },
	{ "connection_timeout", false },
	{ "max_connections", false },
	{ "max_playlist_length", false },
	{ "max_command_list_size", false },
	{ "max_output_buffer_size", false },
	{ "filesystem_charset", false },
	{ "id3v1_encoding", false },
	{ "metadata_to_use", false },
	{ "save_absolute_paths_in_playlists", false },
	{ "gapless_mp3_playback", false },
	{ "auto_update", false },
	{ "auto_update_depth", false },
	{ "despotify_user", false },
	{ "despotify_password", false },
	{ "despotify_high_bitrate", false },
};

static constexpr unsigned n_config_param_templates =
	ARRAY_SIZE(config_param_templates);

static_assert(n_config_param_templates == unsigned(ConfigOption::MAX),
	      "Wrong number of config_param_templates");

const ConfigTemplate config_block_templates[] = {
	{ "audio_output", true },
	{ "decoder", true },
	{ "input", true },
	{ "playlist_plugin", true },
	{ "filter", true },
	{ "database", false },
	{ "neighbors", true },
};

static constexpr unsigned n_config_block_templates =
	ARRAY_SIZE(config_block_templates);

static_assert(n_config_block_templates == unsigned(ConfigBlockOption::MAX),
	      "Wrong number of config_block_templates");

gcc_pure
static inline unsigned
ParseConfigTemplateName(const ConfigTemplate templates[], unsigned count,
			const char *name)
{
	unsigned i = 0;
	for (; i < count; ++i)
		if (strcmp(templates[i].name, name) == 0)
			break;

	return i;
}

ConfigOption
ParseConfigOptionName(const char *name)
{
	return ConfigOption(ParseConfigTemplateName(config_param_templates,
						    n_config_param_templates,
						    name));
}

ConfigBlockOption
ParseConfigBlockOptionName(const char *name)
{
	return ConfigBlockOption(ParseConfigTemplateName(config_block_templates,
							 n_config_block_templates,
							 name));
}
