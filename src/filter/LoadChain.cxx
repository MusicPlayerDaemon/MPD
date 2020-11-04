/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "LoadChain.hxx"
#include "Factory.hxx"
#include "Prepared.hxx"
#include "plugins/ChainFilterPlugin.hxx"
#include "util/IterableSplitString.hxx"

#include <string>

static void
filter_chain_append_new(PreparedFilter &chain, FilterFactory &factory,
			std::string_view template_name)
{
	filter_chain_append(chain, template_name,
			    /* unfortunately, MakeFilter() wants a
			       null-terminated string, so we need to
			       copy it here */
			    factory.MakeFilter(std::string(template_name).c_str()));
}

void
filter_chain_parse(PreparedFilter &chain,
		   FilterFactory &factory,
		   const char *spec)
{
	for (const std::string_view i : IterableSplitString(spec, ',')) {
		if (i.empty())
			continue;

		filter_chain_append_new(chain, factory, i);
	}
}
