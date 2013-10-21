/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "DecoderList.hxx"
#include "DecoderPlugin.hxx"
#include "ConfigGlobal.hxx"
#include "ConfigData.hxx"
#include "decoder/AudiofileDecoderPlugin.hxx"
#include "decoder/PcmDecoderPlugin.hxx"
#include "decoder/DsdiffDecoderPlugin.hxx"
#include "decoder/DsfDecoderPlugin.hxx"
#include "decoder/FlacDecoderPlugin.h"
#include "decoder/OpusDecoderPlugin.h"
#include "decoder/VorbisDecoderPlugin.h"
#include "decoder/AdPlugDecoderPlugin.h"
#include "decoder/WavpackDecoderPlugin.hxx"
#include "decoder/FfmpegDecoderPlugin.hxx"
#include "decoder/GmeDecoderPlugin.hxx"
#include "decoder/FaadDecoderPlugin.hxx"
#include "decoder/MadDecoderPlugin.hxx"
#include "decoder/SndfileDecoderPlugin.hxx"
#include "decoder/Mpg123DecoderPlugin.hxx"
#include "decoder/WildmidiDecoderPlugin.hxx"
#include "decoder/MikmodDecoderPlugin.hxx"
#include "decoder/ModplugDecoderPlugin.hxx"
#include "decoder/MpcdecDecoderPlugin.hxx"
#include "decoder/FluidsynthDecoderPlugin.hxx"
#include "decoder/SidplayDecoderPlugin.hxx"
#include "system/FatalError.hxx"
#include "util/Macros.hxx"

#include <string.h>

const struct DecoderPlugin *const decoder_plugins[] = {
#ifdef HAVE_MAD
	&mad_decoder_plugin,
#endif
#ifdef HAVE_MPG123
	&mpg123_decoder_plugin,
#endif
#ifdef ENABLE_VORBIS_DECODER
	&vorbis_decoder_plugin,
#endif
#if defined(HAVE_FLAC)
	&oggflac_decoder_plugin,
#endif
#ifdef HAVE_FLAC
	&flac_decoder_plugin,
#endif
#ifdef HAVE_OPUS
	&opus_decoder_plugin,
#endif
#ifdef ENABLE_SNDFILE
	&sndfile_decoder_plugin,
#endif
#ifdef HAVE_AUDIOFILE
	&audiofile_decoder_plugin,
#endif
	&dsdiff_decoder_plugin,
	&dsf_decoder_plugin,
#ifdef HAVE_FAAD
	&faad_decoder_plugin,
#endif
#ifdef HAVE_MPCDEC
	&mpcdec_decoder_plugin,
#endif
#ifdef HAVE_WAVPACK
	&wavpack_decoder_plugin,
#endif
#ifdef HAVE_MODPLUG
	&modplug_decoder_plugin,
#endif
#ifdef ENABLE_MIKMOD_DECODER
	&mikmod_decoder_plugin,
#endif
#ifdef ENABLE_SIDPLAY
	&sidplay_decoder_plugin,
#endif
#ifdef ENABLE_WILDMIDI
	&wildmidi_decoder_plugin,
#endif
#ifdef ENABLE_FLUIDSYNTH
	&fluidsynth_decoder_plugin,
#endif
#ifdef HAVE_ADPLUG
	&adplug_decoder_plugin,
#endif
#ifdef HAVE_FFMPEG
	&ffmpeg_decoder_plugin,
#endif
#ifdef HAVE_GME
	&gme_decoder_plugin,
#endif
	&pcm_decoder_plugin,
	nullptr
};

static constexpr unsigned num_decoder_plugins =
	ARRAY_SIZE(decoder_plugins) - 1;

/** which plugins have been initialized successfully? */
bool decoder_plugins_enabled[num_decoder_plugins];

static unsigned
decoder_plugin_index(const struct DecoderPlugin *plugin)
{
	unsigned i = 0;

	while (decoder_plugins[i] != plugin)
		++i;

	return i;
}

static unsigned
decoder_plugin_next_index(const struct DecoderPlugin *plugin)
{
	return plugin == 0
		? 0 /* start with first plugin */
		: decoder_plugin_index(plugin) + 1;
}

const struct DecoderPlugin *
decoder_plugin_from_suffix(const char *suffix,
			   const struct DecoderPlugin *plugin)
{
	if (suffix == nullptr)
		return nullptr;

	for (unsigned i = decoder_plugin_next_index(plugin);
	     decoder_plugins[i] != nullptr; ++i) {
		plugin = decoder_plugins[i];
		if (decoder_plugins_enabled[i] &&
		    plugin->SupportsSuffix(suffix))
			return plugin;
	}

	return nullptr;
}

const struct DecoderPlugin *
decoder_plugin_from_mime_type(const char *mimeType, unsigned int next)
{
	static unsigned i = num_decoder_plugins;

	if (mimeType == nullptr)
		return nullptr;

	if (!next)
		i = 0;
	for (; decoder_plugins[i] != nullptr; ++i) {
		const struct DecoderPlugin *plugin = decoder_plugins[i];
		if (decoder_plugins_enabled[i] &&
		    plugin->SupportsMimeType(mimeType)) {
			++i;
			return plugin;
		}
	}

	return nullptr;
}

const struct DecoderPlugin *
decoder_plugin_from_name(const char *name)
{
	decoder_plugins_find([=](const DecoderPlugin &plugin){
			return strcmp(plugin.name, name) == 0;
		});

	return nullptr;
}

/**
 * Find the "decoder" configuration block for the specified plugin.
 *
 * @param plugin_name the name of the decoder plugin
 * @return the configuration block, or nullptr if none was configured
 */
static const struct config_param *
decoder_plugin_config(const char *plugin_name)
{
	const struct config_param *param = nullptr;

	while ((param = config_get_next_param(CONF_DECODER, param)) != nullptr) {
		const char *name = param->GetBlockValue("plugin");
		if (name == nullptr)
			FormatFatalError("decoder configuration without 'plugin' name in line %d",
					 param->line);

		if (strcmp(name, plugin_name) == 0)
			return param;
	}

	return nullptr;
}

void decoder_plugin_init_all(void)
{
	struct config_param empty;

	for (unsigned i = 0; decoder_plugins[i] != nullptr; ++i) {
		const DecoderPlugin &plugin = *decoder_plugins[i];
		const struct config_param *param =
			decoder_plugin_config(plugin.name);

		if (param == nullptr)
			param = &empty;
		else if (!param->GetBlockValue("enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		if (plugin.Init(*param))
			decoder_plugins_enabled[i] = true;
	}
}

void decoder_plugin_deinit_all(void)
{
	decoder_plugins_for_each_enabled([=](const DecoderPlugin &plugin){
			plugin.Finish();
		});
}
