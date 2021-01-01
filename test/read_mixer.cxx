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

#include "NullMixerListener.hxx"
#include "mixer/MixerControl.hxx"
#include "mixer/MixerList.hxx"
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

	mixer_open(mixer);

	volume = mixer_get_volume(mixer);
	mixer_close(mixer);
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
