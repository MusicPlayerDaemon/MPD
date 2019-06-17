/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "DecoderPrint.hxx"
#include "DecoderList.hxx"
#include "DecoderPlugin.hxx"
#include "client/Response.hxx"

#include <functional>

#include <assert.h>

static void
decoder_plugin_print(Response &r,
		     const DecoderPlugin &plugin)
{
	const char *const*p;

	assert(plugin.name != nullptr);

	r.Format("plugin: %s\n", plugin.name);

	if (plugin.suffixes != nullptr)
		for (p = plugin.suffixes; *p != nullptr; ++p)
			r.Format("suffix: %s\n", *p);

	if (plugin.mime_types != nullptr)
		for (p = plugin.mime_types; *p != nullptr; ++p)
			r.Format("mime_type: %s\n", *p);
}

void
decoder_list_print(Response &r)
{
	using namespace std::placeholders;
	const auto f = std::bind(decoder_plugin_print, std::ref(r), _1);
	decoder_plugins_for_each_enabled(f);
}
