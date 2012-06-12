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
#include "output_list.h"
#include "output_api.h"
#include "output/alsa_output_plugin.h"
#include "output/ao_output_plugin.h"
#include "output/ffado_output_plugin.h"
#include "output/fifo_output_plugin.h"
#include "output/httpd_output_plugin.h"
#include "output/jack_output_plugin.h"
#include "output/mvp_output_plugin.h"
#include "output/null_output_plugin.h"
#include "output/openal_output_plugin.h"
#include "output/oss_output_plugin.h"
#include "output/osx_output_plugin.h"
#include "output/pipe_output_plugin.h"
#include "output/pulse_output_plugin.h"
#include "output/recorder_output_plugin.h"
#include "output/roar_output_plugin.h"
#include "output/shout_output_plugin.h"
#include "output/solaris_output_plugin.h"
#include "output/winmm_output_plugin.h"

const struct audio_output_plugin *const audio_output_plugins[] = {
#ifdef HAVE_SHOUT
	&shout_output_plugin,
#endif
	&null_output_plugin,
#ifdef HAVE_FIFO
	&fifo_output_plugin,
#endif
#ifdef ENABLE_PIPE_OUTPUT
	&pipe_output_plugin,
#endif
#ifdef HAVE_ALSA
	&alsa_output_plugin,
#endif
#ifdef HAVE_ROAR
	&roar_output_plugin,
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
	&osx_output_plugin,
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
#ifdef ENABLE_WINMM_OUTPUT
	&winmm_output_plugin,
#endif
#ifdef ENABLE_FFADO_OUTPUT
	&ffado_output_plugin,
#endif
	NULL
};

const struct audio_output_plugin *
audio_output_plugin_get(const char *name)
{
	audio_output_plugins_for_each(plugin)
		if (strcmp(plugin->name, name) == 0)
			return plugin;

	return NULL;
}
