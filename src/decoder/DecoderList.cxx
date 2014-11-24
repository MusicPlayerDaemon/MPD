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
#include "DecoderList.hxx"
#include "DecoderPlugin.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigData.hxx"
#include "plugins/AudiofileDecoderPlugin.hxx"
#include "plugins/PcmDecoderPlugin.hxx"
#include "plugins/DsdiffDecoderPlugin.hxx"
#include "plugins/DsfDecoderPlugin.hxx"
#include "plugins/FlacDecoderPlugin.h"
#include "plugins/OpusDecoderPlugin.h"
#include "plugins/VorbisDecoderPlugin.h"
#include "plugins/AdPlugDecoderPlugin.h"
#include "plugins/WavpackDecoderPlugin.hxx"
#include "plugins/FfmpegDecoderPlugin.hxx"
#include "plugins/GmeDecoderPlugin.hxx"
#include "plugins/FaadDecoderPlugin.hxx"
#include "plugins/MadDecoderPlugin.hxx"
#include "plugins/SndfileDecoderPlugin.hxx"
#include "plugins/Mpg123DecoderPlugin.hxx"
#include "plugins/WildmidiDecoderPlugin.hxx"
#include "plugins/MikmodDecoderPlugin.hxx"
#include "plugins/ModplugDecoderPlugin.hxx"
#include "plugins/MpcdecDecoderPlugin.hxx"
#include "plugins/FluidsynthDecoderPlugin.hxx"
#include "plugins/SidplayDecoderPlugin.hxx"
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
#ifdef ENABLE_DSD
	&dsdiff_decoder_plugin,
	&dsf_decoder_plugin,
#endif
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

const struct DecoderPlugin *
decoder_plugin_from_name(const char *name)
{
	return decoder_plugins_find([=](const DecoderPlugin &plugin){
			return strcmp(plugin.name, name) == 0;
		});
}

void decoder_plugin_init_all(void)
{
	struct config_param empty;

	for (unsigned i = 0; decoder_plugins[i] != nullptr; ++i) {
		const DecoderPlugin &plugin = *decoder_plugins[i];
		const struct config_param *param =
			config_find_block(CONF_DECODER, "plugin", plugin.name);

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

bool
decoder_plugins_supports_suffix(const char *suffix)
{
	return decoder_plugins_try([suffix](const DecoderPlugin &plugin){
			return plugin.SupportsSuffix(suffix);
		});
}
