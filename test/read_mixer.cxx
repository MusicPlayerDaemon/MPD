/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "mixer/MixerControl.hxx"
#include "mixer/MixerList.hxx"
#include "filter/FilterRegistry.hxx"
#include "pcm/Volume.hxx"
#include "GlobalEvents.hxx"
#include "Main.hxx"
#include "event/Loop.hxx"
#include "config/ConfigData.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_PULSE
#include "output/plugins/PulseOutputPlugin.hxx"

void
pulse_output_lock(gcc_unused PulseOutput *po)
{
}

void
pulse_output_unlock(gcc_unused PulseOutput *po)
{
}

void
pulse_output_set_mixer(gcc_unused PulseOutput *po,
		       gcc_unused PulseMixer *pm)
{
}

void
pulse_output_clear_mixer(gcc_unused PulseOutput *po,
			 gcc_unused PulseMixer *pm)
{
}

bool
pulse_output_set_volume(gcc_unused PulseOutput *po,
			gcc_unused const struct pa_cvolume *volume,
			gcc_unused Error &error)
{
	return false;
}

#endif

#ifdef HAVE_ROAR
#include "output/plugins/RoarOutputPlugin.hxx"

int
roar_output_get_volume(gcc_unused RoarOutput *roar)
{
	return -1;
}

bool
roar_output_set_volume(gcc_unused RoarOutput *roar,
		       gcc_unused unsigned volume)
{
	return true;
}

#endif

void
GlobalEvents::Emit(gcc_unused Event event)
{
}

const struct filter_plugin *
filter_plugin_by_name(gcc_unused const char *name)
{
	assert(false);
	return NULL;
}

int main(int argc, gcc_unused char **argv)
{
	int volume;

	if (argc != 2) {
		fprintf(stderr, "Usage: read_mixer PLUGIN\n");
		return EXIT_FAILURE;
	}

#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif

	EventLoop event_loop;

	Error error;
	Mixer *mixer = mixer_new(event_loop, alsa_mixer_plugin, nullptr,
				 config_param(), error);
	if (mixer == NULL) {
		LogError(error, "mixer_new() failed");
		return EXIT_FAILURE;
	}

	if (!mixer_open(mixer, error)) {
		mixer_free(mixer);
		LogError(error, "failed to open the mixer");
		return EXIT_FAILURE;
	}

	volume = mixer_get_volume(mixer, error);
	mixer_close(mixer);
	mixer_free(mixer);

	assert(volume >= -1 && volume <= 100);

	if (volume < 0) {
		if (error.IsDefined()) {
			LogError(error, "failed to read volume");
		} else
			fprintf(stderr, "failed to read volume\n");
		return EXIT_FAILURE;
	}

	printf("%d\n", volume);
	return 0;
}
