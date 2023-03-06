// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Control.hxx"
#include "Mixer.hxx"

#include <cassert>

Mixer *
mixer_new(EventLoop &event_loop,
	  const MixerPlugin &plugin, AudioOutput &ao,
	  MixerListener &listener,
	  const ConfigBlock &block)
{
	Mixer *mixer = plugin.init(event_loop, ao, listener, block);

	assert(mixer == nullptr || mixer->IsPlugin(plugin));

	return mixer;
}

void
mixer_free(Mixer *mixer) noexcept
{
	assert(mixer != nullptr);

	/* mixers with the "global" flag set might still be open at
	   this point (see Mixer::LockAutoClose()) */
	mixer->LockClose();

	delete mixer;
}
