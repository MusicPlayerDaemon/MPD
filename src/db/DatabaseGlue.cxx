// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"
#include "Interface.hxx"
#include "Registry.hxx"
#include "config/Block.hxx"
#include "lib/fmt/RuntimeError.hxx"

DatabasePtr
DatabaseGlobalInit(EventLoop &main_event_loop,
		   EventLoop &io_event_loop,
		   DatabaseListener &listener,
		   const ConfigBlock &block)
{
	const char *plugin_name =
		block.GetBlockValue("plugin", "simple");

	const DatabasePlugin *plugin = GetDatabasePluginByName(plugin_name);
	if (plugin == nullptr)
		throw FmtRuntimeError("No such database plugin: {}",
				      plugin_name);

	try {
		return plugin->create(main_event_loop, io_event_loop,
				      listener, block);
	} catch (...) {
		std::throw_with_nested(FmtRuntimeError("Failed to initialize database plugin '{}'",
						       plugin_name));
	}
}
