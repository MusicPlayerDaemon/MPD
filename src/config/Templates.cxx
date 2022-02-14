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

#include "Templates.hxx"
#include "Option.hxx"

#include <iterator>

#include <string.h>

const ConfigTemplate config_param_templates[] = {
	{ "music_directory" },
	{ "playlist_directory" },
	{ "follow_inside_symlinks" },
	{ "follow_outside_symlinks" },
	{ "db_file" },
	{ "sticker_file" },
	{ "log_file" },
	{ "pid_file" },
	{ "state_file" },
	{ "state_file_interval" },
	{ "restore_paused" },
	{ "user" },
	{ "group" },
	{ "bind_to_address", true },
	{ "port" },
	{ "log_level" },
	{ "zeroconf_name" },
	{ "zeroconf_enabled" },
	{ "password", true },
	{ "host_permissions", true },
	{ "local_permissions" },
	{ "default_permissions" },
	{ "audio_output_format" },
	{ "mixer_type" },
	{ "replaygain" },
	{ "replaygain_preamp" },
	{ "replaygain_missing_preamp" },
	{ "replaygain_limit" },
	{ "volume_normalization" },
	{ "samplerate_converter" },
	{ "audio_buffer_size" },
	{ "buffer_before_play", false, true },
	{ "http_proxy_host", false, true },
	{ "http_proxy_port", false, true },
	{ "http_proxy_user", false, true },
	{ "http_proxy_password", false, true },
	{ "connection_timeout" },
	{ "max_connections" },
	{ "max_playlist_length" },
	{ "max_command_list_size" },
	{ "max_output_buffer_size" },
	{ "filesystem_charset" },
	{ "id3v1_encoding", false, true },
	{ "metadata_to_use" },
	{ "save_absolute_paths_in_playlists" },
	{ "gapless_mp3_playback", false, true },
	{ "auto_update" },
	{ "auto_update_depth" },
	{ "despotify_user", false, true },
	{ "despotify_password", false, true },
	{ "despotify_high_bitrate", false, true },
	{ "mixramp_analyzer" },
};

static constexpr unsigned n_config_param_templates =
	std::size(config_param_templates);

static_assert(n_config_param_templates == unsigned(ConfigOption::MAX),
	      "Wrong number of config_param_templates");

const ConfigTemplate config_block_templates[] = {
	{ "audio_output", true },
	{ "decoder", true },
	{ "input", true },
	{ "input_cache" },
	{ "archive_plugin", true },
	{ "playlist_plugin", true },
	{ "resampler" },
	{ "filter", true },
	{ "database" },
	{ "neighbors", true },
};

static constexpr unsigned n_config_block_templates =
	std::size(config_block_templates);

static_assert(n_config_block_templates == unsigned(ConfigBlockOption::MAX),
	      "Wrong number of config_block_templates");

gcc_pure
static inline unsigned
ParseConfigTemplateName(const ConfigTemplate templates[], unsigned count,
			const char *name) noexcept
{
	unsigned i = 0;
	for (; i < count; ++i)
		if (strcmp(templates[i].name, name) == 0)
			break;

	return i;
}

ConfigOption
ParseConfigOptionName(const char *name) noexcept
{
	return ConfigOption(ParseConfigTemplateName(config_param_templates,
						    n_config_param_templates,
						    name));
}

ConfigBlockOption
ParseConfigBlockOptionName(const char *name) noexcept
{
	return ConfigBlockOption(ParseConfigTemplateName(config_block_templates,
							 n_config_block_templates,
							 name));
}
