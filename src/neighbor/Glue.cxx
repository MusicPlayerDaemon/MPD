/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "Glue.hxx"
#include "Registry.hxx"
#include "Explorer.hxx"
#include "NeighborPlugin.hxx"
#include "Info.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigError.hxx"
#include "config/Block.hxx"
#include "util/Error.hxx"

NeighborGlue::Explorer::~Explorer()
{
	delete explorer;
}

NeighborGlue::~NeighborGlue() {}

static NeighborExplorer *
CreateNeighborExplorer(EventLoop &loop, NeighborListener &listener,
		       const ConfigBlock &block, Error &error)
{
	const char *plugin_name = block.GetBlockValue("plugin");
	if (plugin_name == nullptr) {
		error.Set(config_domain,
			  "Missing \"plugin\" configuration");
		return nullptr;
	}

	const NeighborPlugin *plugin = GetNeighborPluginByName(plugin_name);
	if (plugin == nullptr) {
		error.Format(config_domain, "No such neighbor plugin: %s",
			     plugin_name);
		return nullptr;
	}

	return plugin->create(loop, listener, block, error);
}

bool
NeighborGlue::Init(EventLoop &loop, NeighborListener &listener, Error &error)
{
	for (const auto *block = config_get_block(ConfigBlockOption::NEIGHBORS);
	     block != nullptr; block = block->next) {
		NeighborExplorer *explorer =
			CreateNeighborExplorer(loop, listener, *block, error);
		if (explorer == nullptr) {
			error.FormatPrefix("Line %i: ", block->line);
			return false;
		}

		explorers.emplace_front(explorer);
	}

	return true;
}

bool
NeighborGlue::Open(Error &error)
{
	for (auto i = explorers.begin(), end = explorers.end();
	     i != end; ++i) {
		if (!i->explorer->Open(error)) {
			/* roll back */
			for (auto k = explorers.begin(); k != i; ++k)
				k->explorer->Close();
			return false;
		}
	}

	return true;
}

void
NeighborGlue::Close()
{
	for (auto i = explorers.begin(), end = explorers.end(); i != end; ++i)
		i->explorer->Close();
}

NeighborGlue::List
NeighborGlue::GetList() const
{
	List result;

	for (const auto &i : explorers)
		result.splice_after(result.before_begin(),
				    i.explorer->GetList());

	return result;
}

