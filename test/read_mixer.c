/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "mixer_control.h"
#include "mixer_list.h"
#include "filter_registry.h"
#include "pcm_volume.h"
#include "event_pipe.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_PULSE
#include "output/pulse_output_plugin.h"

void
pulse_output_set_mixer(G_GNUC_UNUSED struct pulse_output *po,
		       G_GNUC_UNUSED struct pulse_mixer *pm)
{
}

void
pulse_output_clear_mixer(G_GNUC_UNUSED struct pulse_output *po,
			 G_GNUC_UNUSED struct pulse_mixer *pm)
{
}

bool
pulse_output_set_volume(G_GNUC_UNUSED struct pulse_output *po,
			G_GNUC_UNUSED const struct pa_cvolume *volume,
			G_GNUC_UNUSED GError **error_r)
{
	return false;
}

#endif

void
event_pipe_emit(G_GNUC_UNUSED enum pipe_event event)
{
}

const struct filter_plugin *
filter_plugin_by_name(G_GNUC_UNUSED const char *name)
{
	assert(false);
	return NULL;
}

bool
pcm_volume(G_GNUC_UNUSED void *buffer, G_GNUC_UNUSED int length,
	   G_GNUC_UNUSED const struct audio_format *format,
	   G_GNUC_UNUSED int volume)
{
	assert(false);
	return false;
}

int main(int argc, G_GNUC_UNUSED char **argv)
{
	GError *error = NULL;
	struct mixer *mixer;
	bool success;
	int volume;

	if (argc != 2) {
		g_printerr("Usage: read_mixer PLUGIN\n");
		return 1;
	}

	g_thread_init(NULL);

	mixer = mixer_new(&alsa_mixer_plugin, NULL, NULL, &error);
	if (mixer == NULL) {
		g_printerr("mixer_new() failed: %s\n", error->message);
		g_error_free(error);
		return 2;
	}

	success = mixer_open(mixer, &error);
	if (!success) {
		mixer_free(mixer);
		g_printerr("failed to open the mixer: %s\n", error->message);
		g_error_free(error);
		return 2;
	}

	volume = mixer_get_volume(mixer, &error);
	mixer_close(mixer);
	mixer_free(mixer);

	assert(volume >= -1 && volume <= 100);

	if (volume < 0) {
		if (error != NULL) {
			g_printerr("failed to read volume: %s\n",
				   error->message);
			g_error_free(error);
		} else
			g_printerr("failed to read volume\n");
		return 2;
	}

	g_print("%d\n", volume);
	return 0;
}
