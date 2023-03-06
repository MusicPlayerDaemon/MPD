// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LoadOne.hxx"
#include "FilterPlugin.hxx"
#include "Registry.hxx"
#include "Prepared.hxx"
#include "config/Block.hxx"
#include "lib/fmt/RuntimeError.hxx"

std::unique_ptr<PreparedFilter>
filter_configured_new(const ConfigBlock &block)
{
	const char *plugin_name = block.GetBlockValue("plugin");
	if (plugin_name == nullptr)
		throw std::runtime_error("No filter plugin specified");

	const auto *plugin = filter_plugin_by_name(plugin_name);
	if (plugin == nullptr)
		throw FmtRuntimeError("No such filter plugin: {}",
				      plugin_name);

	return plugin->init(block);
}
