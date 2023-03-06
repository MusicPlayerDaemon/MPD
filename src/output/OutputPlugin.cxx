// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "OutputPlugin.hxx"

#include <cassert>

AudioOutput *
ao_plugin_init(EventLoop &event_loop,
	       const AudioOutputPlugin &plugin,
	       const ConfigBlock &block)
{
	assert(plugin.init != nullptr);

	return plugin.init(event_loop, block);
}
