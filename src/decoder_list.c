/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#include "decoder_list.h"
#include "decoder_plugin.h"
#include "utils.h"
#include "config.h"

#include <glib.h>

#include <string.h>

extern const struct decoder_plugin mp3Plugin;
extern const struct decoder_plugin vorbis_decoder_plugin;
extern const struct decoder_plugin flac_decoder_plugin;
extern const struct decoder_plugin oggflac_decoder_plugin;
extern const struct decoder_plugin audiofilePlugin;
extern const struct decoder_plugin mp4_plugin;
extern const struct decoder_plugin aacPlugin;
extern const struct decoder_plugin mpcPlugin;
extern const struct decoder_plugin wavpack_plugin;
extern const struct decoder_plugin modplug_plugin;
extern const struct decoder_plugin mikmod_decoder_plugin;
extern const struct decoder_plugin sidplay_decoder_plugin;
extern const struct decoder_plugin fluidsynth_decoder_plugin;
extern const struct decoder_plugin wildmidi_decoder_plugin;
extern const struct decoder_plugin ffmpeg_plugin;

static const struct decoder_plugin *const decoder_plugins[] = {
#ifdef HAVE_MAD
	&mp3Plugin,
#endif
#ifdef HAVE_OGGVORBIS
	&vorbis_decoder_plugin,
#endif
#if defined(HAVE_FLAC) || defined(HAVE_OGGFLAC)
	&oggflac_decoder_plugin,
#endif
#ifdef HAVE_FLAC
	&flac_decoder_plugin,
#endif
#ifdef HAVE_AUDIOFILE
	&audiofilePlugin,
#endif
#ifdef HAVE_FAAD
	&aacPlugin,
#endif
#ifdef HAVE_MP4
	&mp4_plugin,
#endif
#ifdef HAVE_MPCDEC
	&mpcPlugin,
#endif
#ifdef HAVE_WAVPACK
	&wavpack_plugin,
#endif
#ifdef HAVE_MODPLUG
	&modplug_plugin,
#endif
#ifdef HAVE_MIKMOD
	&mikmod_decoder_plugin,
#endif
#ifdef ENABLE_SIDPLAY
	&sidplay_decoder_plugin,
#endif
#ifdef ENABLE_FLUIDSYNTH
	&fluidsynth_decoder_plugin,
#endif
#ifdef ENABLE_WILDMIDI
	&wildmidi_decoder_plugin,
#endif
#ifdef HAVE_FFMPEG
	&ffmpeg_plugin,
#endif
};

enum {
	num_decoder_plugins = G_N_ELEMENTS(decoder_plugins),
};

/** which plugins have been initialized successfully? */
static bool decoder_plugins_enabled[num_decoder_plugins];

const struct decoder_plugin *
decoder_plugin_from_suffix(const char *suffix, unsigned int next)
{
	static unsigned i = num_decoder_plugins;

	if (suffix == NULL)
		return NULL;

	if (!next)
		i = 0;
	for (; i < num_decoder_plugins; ++i) {
		const struct decoder_plugin *plugin = decoder_plugins[i];
		if (decoder_plugins_enabled[i] &&
		    stringFoundInStringArray(plugin->suffixes, suffix)) {
			++i;
			return plugin;
		}
	}

	return NULL;
}

const struct decoder_plugin *
decoder_plugin_from_mime_type(const char *mimeType, unsigned int next)
{
	static unsigned i = num_decoder_plugins;

	if (mimeType == NULL)
		return NULL;

	if (!next)
		i = 0;
	for (; i < num_decoder_plugins; ++i) {
		const struct decoder_plugin *plugin = decoder_plugins[i];
		if (decoder_plugins_enabled[i] &&
		    stringFoundInStringArray(plugin->mime_types, mimeType)) {
			++i;
			return plugin;
		}
	}

	return NULL;
}

const struct decoder_plugin *
decoder_plugin_from_name(const char *name)
{
	for (unsigned i = 0; i < num_decoder_plugins; ++i) {
		const struct decoder_plugin *plugin = decoder_plugins[i];
		if (decoder_plugins_enabled[i] &&
		    strcmp(plugin->name, name) == 0)
			return plugin;
	}

	return NULL;
}

void decoder_plugin_print_all_suffixes(FILE * fp)
{
	const char *const*suffixes;

	for (unsigned i = 0; i < num_decoder_plugins; ++i) {
		const struct decoder_plugin *plugin = decoder_plugins[i];
		if (!decoder_plugins_enabled[i])
			continue;

		suffixes = plugin->suffixes;
		while (suffixes && *suffixes) {
			fprintf(fp, "%s ", *suffixes);
			suffixes++;
		}
	}
	fprintf(fp, "\n");
	fflush(fp);
}

void decoder_plugin_print_all_decoders(FILE * fp)
{
	for (unsigned i = 0; i < num_decoder_plugins; ++i) {
		const struct decoder_plugin *plugin = decoder_plugins[i];
		if (!decoder_plugins_enabled[i])
			continue;

		fprintf(fp, "%s ", plugin->name);
	}
	fprintf(fp, "\n");
	fflush(fp);
}

void decoder_plugin_init_all(void)
{
	for (unsigned i = 0; i < num_decoder_plugins; ++i) {
		const struct decoder_plugin *plugin = decoder_plugins[i];

		if (decoder_plugin_init(plugin, NULL))
			decoder_plugins_enabled[i] = true;
	}
}

void decoder_plugin_deinit_all(void)
{
	for (unsigned i = 0; i < num_decoder_plugins; ++i) {
		const struct decoder_plugin *plugin = decoder_plugins[i];

		if (decoder_plugins_enabled[i])
			decoder_plugin_finish(plugin);
	}
}
