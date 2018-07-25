/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "LoadChain.hxx"
#include "LoadOne.hxx"
#include "Prepared.hxx"
#include "plugins/ChainFilterPlugin.hxx"
#include "config/Param.hxx"
#include "config/Option.hxx"
#include "config/Data.hxx"
#include "config/Domain.hxx"
#include "config/Block.hxx"
#include "util/RuntimeError.hxx"

#include <algorithm>

#include <string.h>

static void
filter_chain_append_new(PreparedFilter &chain, const ConfigData &config,
			const char *template_name)
{
	const auto *cfg = config.FindBlock(ConfigBlockOption::AUDIO_FILTER,
					   "name", template_name);
	if (cfg == nullptr)
		throw FormatRuntimeError("Filter template not found: %s",
					 template_name);

	cfg->SetUsed();

	// Instantiate one of those filter plugins with the template name as a hint
	auto f = filter_configured_new(*cfg);

	const char *plugin_name = cfg->GetBlockValue("plugin",
						     "unknown");
	filter_chain_append(chain, plugin_name, std::move(f));
}

void
filter_chain_parse(PreparedFilter &chain,
		   const ConfigData &config,
		   const char *spec)
{
	const char *const end = spec + strlen(spec);

	while (true) {
		const char *comma = std::find(spec, end, ',');
		if (comma > spec) {
			const std::string name(spec, comma);
			filter_chain_append_new(chain, config, name.c_str());
		}

		if (comma == end)
			break;

		spec = comma + 1;
	}
}
