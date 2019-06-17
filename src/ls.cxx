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
#include "util/ASCII.hxx"
#include "util/UriUtil.hxx"

#include <assert.h>

void print_supported_uri_schemes_to_fp(FILE *fp)
{
#ifdef HAVE_UN
	fprintf(fp, " file://");
#endif
	input_plugins_for_each(plugin)
		for (auto i = plugin->prefixes; *i != nullptr; ++i)
			fprintf(fp, " %s", *i);
	fprintf(fp,"\n");
}

void
print_supported_uri_schemes(Response &r)
{
	input_plugins_for_each_enabled(plugin)
		for (auto i = plugin->prefixes; *i != nullptr; ++i)
			r.Format("handler: %s\n", *i);
}

bool
uri_supported_scheme(const char *uri) noexcept
{
	assert(uri_has_scheme(uri));

	input_plugins_for_each_enabled(plugin)
		for (auto i = plugin->prefixes; *i != nullptr; ++i)
			if (StringStartsWithCaseASCII(uri, *i))
				return true;

	return false;
}
