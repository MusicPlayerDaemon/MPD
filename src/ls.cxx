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

#include "config.h"
#include "ls.hxx"
#include "input/Registry.hxx"
#include "input/InputPlugin.hxx"
#include "client/Response.hxx"
#include "util/UriExtract.hxx"

#include <assert.h>

#include <string>

void print_supported_uri_schemes_to_fp(FILE *fp)
{
#ifdef HAVE_UN
	fprintf(fp, " file://");
#endif
	std::set<std::string> protocols;
	input_plugins_for_each(plugin)
		plugin->ForeachSupportedUri([&](const char* uri) {
			protocols.emplace(uri);
		});

	for (auto protocol : protocols) {
		fprintf(fp, " %s", protocol.c_str());
	}
	fprintf(fp,"\n");
}

void
print_supported_uri_schemes(Response &r)
{
	std::set<std::string> protocols;
	input_plugins_for_each_enabled(plugin)
		plugin->ForeachSupportedUri([&](const char* uri) {
			protocols.emplace(uri);
		});

	for (auto protocol : protocols) {
		r.Format("handler: %s\n", protocol.c_str());
	}
}

bool
uri_supported_scheme(const char *uri) noexcept
{
	assert(uri_has_scheme(uri));

	input_plugins_for_each_enabled(plugin)
		if (plugin->SupportsUri(uri))
			return true;

	return false;
}
