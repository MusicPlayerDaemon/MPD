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

#include "ConfigTemplates.hxx"
#include "ConfigOption.hxx"

#include <string.h>

const ConfigTemplate config_templates[] = {
	{ "music_directory", false, false },
	{ "playlist_directory", false, false },
	{ "follow_inside_symlinks", false, false },
	{ "follow_outside_symlinks", false, false },
	{ "db_file", false, false },
	{ "sticker_file", false, false },
	{ "log_file", false, false },
	{ "pid_file", false, false },
	{ "state_file", false, false },
	{ "state_file_interval", false, false },
	{ "restore_paused", false, false },
	{ "user", false, false },
	{ "group", false, false },
	{ "bind_to_address", true, false },
	{ "port", false, false },
	{ "log_level", false, false },
	{ "zeroconf_name", false, false },
	{ "zeroconf_enabled", false, false },
	{ "password", true, false },
	{ "default_permissions", false, false },
	{ "audio_output", true, true },
	{ "audio_output_format", false, false },
	{ "mixer_type", false, false },
	{ "replaygain", false, false },
	{ "replaygain_preamp", false, false },
	{ "replaygain_missing_preamp", false, false },
	{ "replaygain_limit", false, false },
	{ "volume_normalization", false, false },
	{ "samplerate_converter", false, false },
	{ "audio_buffer_size", false, false },
	{ "buffer_before_play", false, false },
	{ "http_proxy_host", false, false },
	{ "http_proxy_port", false, false },
	{ "http_proxy_user", false, false },
	{ "http_proxy_password", false, false },
	{ "connection_timeout", false, false },
	{ "max_connections", false, false },
	{ "max_playlist_length", false, false },
	{ "max_command_list_size", false, false },
	{ "max_output_buffer_size", false, false },
	{ "filesystem_charset", false, false },
	{ "id3v1_encoding", false, false },
	{ "metadata_to_use", false, false },
	{ "save_absolute_paths_in_playlists", false, false },
	{ "decoder", true, true },
	{ "input", true, true },
	{ "gapless_mp3_playback", false, false },
	{ "playlist_plugin", true, true },
	{ "auto_update", false, false },
	{ "auto_update_depth", false, false },
	{ "despotify_user", false, false },
	{ "despotify_password", false, false},
	{ "despotify_high_bitrate", false, false },
	{ "filter", true, true },
	{ "database", false, true },
	{ "neighbors", true, true },
};

static constexpr unsigned n_config_templates =
	sizeof(config_templates) / sizeof(config_templates[0]);

static_assert(n_config_templates == unsigned(CONF_MAX),
	      "Wrong number of config_templates");

ConfigOption
ParseConfigOptionName(const char *name)
{
	for (unsigned i = 0; i < n_config_templates; ++i)
		if (strcmp(config_templates[i].name, name) == 0)
			return ConfigOption(i);

	return CONF_MAX;
}
