/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "output_list.h"
#include "output_api.h"
#include "os_compat.h"

extern const struct audio_output_plugin shoutPlugin;
extern const struct audio_output_plugin nullPlugin;
extern const struct audio_output_plugin fifoPlugin;
extern const struct audio_output_plugin alsaPlugin;
extern const struct audio_output_plugin aoPlugin;
extern const struct audio_output_plugin ossPlugin;
extern const struct audio_output_plugin osxPlugin;
extern const struct audio_output_plugin pulse_plugin;
extern const struct audio_output_plugin mvpPlugin;
extern const struct audio_output_plugin jackPlugin;

const struct audio_output_plugin *audio_output_plugins[] = {
#ifdef HAVE_SHOUT
	&shoutPlugin,
#endif
	&nullPlugin,
#ifdef HAVE_FIFO
	&fifoPlugin,
#endif
#ifdef HAVE_ALSA
	&alsaPlugin,
#endif
#ifdef HAVE_AO
	&aoPlugin,
#endif
#ifdef HAVE_OSS
	&ossPlugin,
#endif
#ifdef HAVE_OSX
	&osxPlugin,
#endif
#ifdef HAVE_PULSE
	&pulse_plugin,
#endif
#ifdef HAVE_MVP
	&mvpPlugin,
#endif
#ifdef HAVE_JACK
	&jackPlugin,
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
