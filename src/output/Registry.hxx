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

#ifndef MPD_OUTPUT_LIST_HXX
#define MPD_OUTPUT_LIST_HXX

struct AudioOutputPlugin;

extern const AudioOutputPlugin *const audio_output_plugins[];

const AudioOutputPlugin *
AudioOutputPlugin_get(const char *name);

#define audio_output_plugins_for_each(plugin) \
	for (const AudioOutputPlugin *plugin, \
		*const*output_plugin_iterator = &audio_output_plugins[0]; \
		(plugin = *output_plugin_iterator) != nullptr; ++output_plugin_iterator)

#endif
