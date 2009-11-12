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

#include "config.h"
#include "decoder_plugin.h"
#include "utils.h"

#include <assert.h>

bool
decoder_plugin_supports_suffix(const struct decoder_plugin *plugin,
			       const char *suffix)
{
	assert(plugin != NULL);
	assert(suffix != NULL);

	return plugin->suffixes != NULL &&
		string_array_contains(plugin->suffixes, suffix);

}

bool
decoder_plugin_supports_mime_type(const struct decoder_plugin *plugin,
				  const char *mime_type)
{
	assert(plugin != NULL);
	assert(mime_type != NULL);

	return plugin->mime_types != NULL &&
		string_array_contains(plugin->mime_types, mime_type);
}
