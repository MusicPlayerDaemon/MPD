/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "MixerControl.hxx"
#include "MixerList.hxx"
#include "FilterRegistry.hxx"
#include "pcm/Volume.hxx"
#include "GlobalEvents.hxx"
#include "Main.hxx"
#include "event/Loop.hxx"
#include "ConfigData.hxx"
#include "util/Error.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>

EventLoop *main_loop;

#ifdef HAVE_PULSE
#include "output/PulseOutputPlugin.hxx"

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
#include "output/RoarOutputPlugin.hxx"

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

bool
pcm_volume(gcc_unused void *buffer, gcc_unused size_t length,
	   gcc_unused SampleFormat format,
	   gcc_unused int volume)
{
	assert(false);
	return false;
}

int main(int argc, gcc_unused char **argv)
{
	int volume;

	if (argc != 2) {
		g_printerr("Usage: read_mixer PLUGIN\n");
		return 1;
	}

#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif

	main_loop = new EventLoop(EventLoop::Default());

	Error error;
	Mixer *mixer = mixer_new(&alsa_mixer_plugin, nullptr,
				 config_param(), error);
	if (mixer == NULL) {
		g_printerr("mixer_new() failed: %s\n", error.GetMessage());
		return 2;
	}

	if (!mixer_open(mixer, error)) {
		mixer_free(mixer);
		g_printerr("failed to open the mixer: %s\n", error.GetMessage());
		return 2;
	}

	volume = mixer_get_volume(mixer, error);
	mixer_close(mixer);
	mixer_free(mixer);

	delete main_loop;

	assert(volume >= -1 && volume <= 100);

	if (volume < 0) {
		if (error.IsDefined()) {
			g_printerr("failed to read volume: %s\n",
				   error.GetMessage());
		} else
			g_printerr("failed to read volume\n");
		return 2;
	}

	g_print("%d\n", volume);
	return 0;
}
