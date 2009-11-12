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
#include "output_list.h"
#include "output_api.h"

extern const struct audio_output_plugin shoutPlugin;
extern const struct audio_output_plugin null_output_plugin;
extern const struct audio_output_plugin fifo_output_plugin;
extern const struct audio_output_plugin pipe_output_plugin;
extern const struct audio_output_plugin alsaPlugin;
extern const struct audio_output_plugin ao_output_plugin;
extern const struct audio_output_plugin oss_output_plugin;
extern const struct audio_output_plugin openal_output_plugin;
extern const struct audio_output_plugin osxPlugin;
extern const struct audio_output_plugin solaris_output_plugin;
extern const struct audio_output_plugin pulse_output_plugin;
extern const struct audio_output_plugin mvp_output_plugin;
extern const struct audio_output_plugin jack_output_plugin;
extern const struct audio_output_plugin httpd_output_plugin;
extern const struct audio_output_plugin recorder_output_plugin;

const struct audio_output_plugin *audio_output_plugins[] = {
#ifdef HAVE_SHOUT
	&shoutPlugin,
#endif
	&null_output_plugin,
#ifdef HAVE_FIFO
	&fifo_output_plugin,
#endif
#ifdef ENABLE_PIPE_OUTPUT
	&pipe_output_plugin,
#endif
#ifdef HAVE_ALSA
	&alsaPlugin,
#endif
#ifdef HAVE_AO
	&ao_output_plugin,
#endif
#ifdef HAVE_OSS
	&oss_output_plugin,
#endif
#ifdef HAVE_OPENAL
	&openal_output_plugin,
#endif
#ifdef HAVE_OSX
	&osxPlugin,
#endif
#ifdef ENABLE_SOLARIS_OUTPUT
	&solaris_output_plugin,
#endif
#ifdef HAVE_PULSE
	&pulse_output_plugin,
#endif
#ifdef HAVE_MVP
	&mvp_output_plugin,
#endif
#ifdef HAVE_JACK
	&jack_output_plugin,
#endif
#ifdef ENABLE_HTTPD_OUTPUT
	&httpd_output_plugin,
#endif
#ifdef ENABLE_RECORDER_OUTPUT
	&recorder_output_plugin,
#endif
	NULL
};

const struct audio_output_plugin *
audio_output_plugin_get(const char *name)
{
	unsigned int i;
	const struct audio_output_plugin *plugin;

	audio_output_plugins_for_each(plugin, i)
		if (strcmp(audio_output_plugins[i]->name, name) == 0)
			return audio_output_plugins[i];

	return NULL;
}

void audio_output_plugin_print_all_types(FILE * fp)
{
	unsigned i;
	const struct audio_output_plugin *plugin;

	audio_output_plugins_for_each(plugin, i)
		fprintf(fp, "%s ", plugin->name);

	fprintf(fp, "\n");
	fflush(fp);
}
