// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "NullMixerListener.hxx"
#include "mixer/Control.hxx"
#include "mixer/Mixer.hxx"
#include "mixer/plugins/AlsaMixerPlugin.hxx"
#include "filter/Registry.hxx"
#include "event/Loop.hxx"
#include "config/Block.hxx"
#include "util/PrintException.hxx"

#include <cassert>

#include <stdlib.h>

const FilterPlugin *
filter_plugin_by_name([[maybe_unused]] const char *name) noexcept
{
	assert(false);
	return nullptr;
}

int main(int argc, [[maybe_unused]] char **argv)
try {
	int volume;

	if (argc != 2) {
		fprintf(stderr, "Usage: read_mixer PLUGIN\n");
		return EXIT_FAILURE;
	}

	EventLoop event_loop;

	NullMixerListener mixer_listener;
	Mixer *mixer = mixer_new(event_loop, alsa_mixer_plugin,
				 /* ugly dangerous dummy pointer to
				    make the compiler happy; this
				    works with most mixers, because
				    they don't need the AudioOutput */
				 *(AudioOutput *)0x1,
				 mixer_listener,
				 ConfigBlock());

	volume = mixer->LockGetVolume();
	mixer_free(mixer);

	assert(volume >= -1 && volume <= 100);

	if (volume < 0) {
		fprintf(stderr, "failed to read volume\n");
		return EXIT_FAILURE;
	}

	printf("%d\n", volume);
	return 0;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
