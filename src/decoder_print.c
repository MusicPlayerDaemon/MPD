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
#include "decoder_print.h"
#include "decoder_list.h"
#include "decoder_plugin.h"
#include "client.h"

#include <assert.h>

static void
decoder_plugin_print(struct client *client,
		     const struct decoder_plugin *plugin)
{
	const char *const*p;

	assert(plugin != NULL);
	assert(plugin->name != NULL);

	client_printf(client, "plugin: %s\n", plugin->name);

	if (plugin->suffixes != NULL)
		for (p = plugin->suffixes; *p != NULL; ++p)
			client_printf(client, "suffix: %s\n", *p);

	if (plugin->mime_types != NULL)
		for (p = plugin->mime_types; *p != NULL; ++p)
			client_printf(client, "mime_type: %s\n", *p);
}

void
decoder_list_print(struct client *client)
{
	for (unsigned i = 0; decoder_plugins[i] != NULL; ++i)
		if (decoder_plugins_enabled[i])
			decoder_plugin_print(client, decoder_plugins[i]);
}
