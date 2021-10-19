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

#include "Glue.hxx"
#include "Registry.hxx"
#include "Explorer.hxx"
#include "NeighborPlugin.hxx"
#include "Info.hxx"
#include "config/Data.hxx"
#include "config/Block.hxx"
#include "util/RuntimeError.hxx"

#include <stdexcept>

NeighborGlue::NeighborGlue() noexcept = default;
NeighborGlue::~NeighborGlue() noexcept = default;

static std::unique_ptr<NeighborExplorer>
CreateNeighborExplorer(EventLoop &loop, NeighborListener &listener,
		       const char *plugin_name,
		       const ConfigBlock &block)
{
	const NeighborPlugin *plugin = GetNeighborPluginByName(plugin_name);
	if (plugin == nullptr)
		throw FormatRuntimeError("No such neighbor plugin: %s",
					 plugin_name);

	return plugin->create(loop, listener, block);
}

void
NeighborGlue::Init(const ConfigData &config,
		   EventLoop &loop, NeighborListener &listener)
{
	for (const auto &block : config.GetBlockList(ConfigBlockOption::NEIGHBORS)) {
		block.SetUsed();

		try {
			const char *plugin_name = block.GetBlockValue("plugin");
			if (plugin_name == nullptr)
				throw std::runtime_error("Missing \"plugin\" configuration");

			explorers.emplace_front(plugin_name,
						CreateNeighborExplorer(loop,
								       listener,
								       plugin_name,
								       block));
		} catch (...) {
			std::throw_with_nested(FormatRuntimeError("Line %i: ",
								  block.line));
		}
	}
}

void
NeighborGlue::Open()
{
	for (auto i = explorers.begin(), end = explorers.end();
	     i != end; ++i) {
		try {
			i->explorer->Open();
		} catch (...) {
			/* roll back */
			for (auto k = explorers.begin(); k != i; ++k)
				k->explorer->Close();

			std::throw_with_nested(FormatRuntimeError("Failed to open neighblor plugin '%s'",
								  i->name.c_str()));
		}
	}
}

void
NeighborGlue::Close() noexcept
{
	for (auto & explorer : explorers)
		explorer.explorer->Close();
}

NeighborGlue::List
NeighborGlue::GetList() const noexcept
{
	List result;

	for (const auto &i : explorers)
		result.splice_after(result.before_begin(),
				    i.explorer->GetList());

	return result;
}

