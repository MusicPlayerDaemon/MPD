// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Init.hxx"
#include "Registry.hxx"
#include "InputPlugin.hxx"
#include "BufferedInputStream.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "config/Block.hxx"
#include "config/Parser.hxx"
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
static constexpr size_t KILOBYTE = 1024;

void
input_stream_global_init(const ConfigData &config, EventLoop &event_loop)
{
#ifdef HAVE_URING
	InitUringInputPlugin(event_loop);
#endif

	if (auto *param = config.GetParam(ConfigOption::MAX_BUFFERED_INPUT_STREAM_SIZE)) {
		offset_type buffer_size = param->With([](const char *s){
			size_t result = ParseSize(s, KILOBYTE);
			if (result <= 0)
				throw FmtRuntimeError("max buffered input size \"{}\" is not a positive integer", s);
			return result;
		});
		BufferedInputStream::SetMaxSize(buffer_size);
	}

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
				 "Input plugin '{}' is not configured: {}",
				 plugin->name, e.what());
			continue;
		} catch (const PluginUnavailable &e) {
			FmtDebug(input_domain,
				 "Input plugin '{}' is unavailable: {}",
				 plugin->name, e.what());
			continue;
		} catch (...) {
			std::throw_with_nested(FmtRuntimeError("Failed to initialize input plugin '{}'",
							       plugin->name));
		}
	}
}

void
input_stream_global_finish() noexcept
{
	input_plugins_for_each_enabled(plugin)
		if (plugin->finish != nullptr)
			plugin->finish();
}
