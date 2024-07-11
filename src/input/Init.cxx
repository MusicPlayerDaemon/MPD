// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Init.hxx"
#include "Registry.hxx"
#include "InputPlugin.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "config/Block.hxx"
#include "Log.hxx"
#include "PluginUnavailable.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/Domain.hxx"

#include <cassert>
#include <stdexcept>

#include "io/uring/Features.h"
#ifdef HAVE_URING
#include "plugins/UringInputPlugin.hxx"
#endif

static constexpr Domain input_domain("input");

void
input_stream_global_init(const ConfigData &config, EventLoop &event_loop)
{
#ifdef HAVE_URING
	InitUringInputPlugin(event_loop);
#endif

	const ConfigBlock empty;

	for (unsigned i = 0; input_plugins[i] != nullptr; ++i) {
		const InputPlugin *plugin = input_plugins[i];

		assert(plugin->name != nullptr);
		assert(*plugin->name != 0);
		assert(plugin->open != nullptr);

		const auto *block =
			config.FindBlock(ConfigBlockOption::INPUT, "plugin",
					 plugin->name);
		if (block == nullptr) {
			block = &empty;
		} else if (!block->GetBlockValue("enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		block->SetUsed();

		try {
			if (plugin->init != nullptr)
				plugin->init(event_loop, *block);
			input_plugins_enabled[i] = true;
		} catch (const PluginUnconfigured &e) {
			FmtDebug(input_domain,
				 "Input plugin {:?} is not configured: {}",
				 plugin->name, e.what());
			continue;
		} catch (const PluginUnavailable &e) {
			FmtDebug(input_domain,
				 "Input plugin {:?} is unavailable: {}",
				 plugin->name, e.what());
			continue;
		} catch (...) {
			std::throw_with_nested(FmtRuntimeError("Failed to initialize input plugin {:?}",
							       plugin->name));
		}
	}
}

void
input_stream_global_finish() noexcept
{
	for (const auto &plugin : GetEnabledInputPlugins())
		if (plugin.finish != nullptr)
			plugin.finish();
}
