/*
 * Copyright 2003-2019 The Music Player Daemon Project
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
#include "PluginUnavailable.hxx"
#include "Log.hxx"
#include "config/Data.hxx"
#include "config/Block.hxx"
#include "plugins/AudiofileDecoderPlugin.hxx"
#include "plugins/PcmDecoderPlugin.hxx"
#include "plugins/DsdiffDecoderPlugin.hxx"
#include "plugins/DsfDecoderPlugin.hxx"
#include "plugins/HybridDsdDecoderPlugin.hxx"
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
#include "util/RuntimeError.hxx"

#include <iterator>

#include <string.h>

const struct DecoderPlugin *const decoder_plugins[] = {
#ifdef ENABLE_MAD
	&mad_decoder_plugin,
#endif
#ifdef ENABLE_MPG123
	&mpg123_decoder_plugin,
#endif
#ifdef ENABLE_VORBIS_DECODER
	&vorbis_decoder_plugin,
#endif
#ifdef ENABLE_FLAC
	&oggflac_decoder_plugin,
	&flac_decoder_plugin,
#endif
#ifdef ENABLE_OPUS
	&opus_decoder_plugin,
#endif
#ifdef ENABLE_SNDFILE
	&sndfile_decoder_plugin,
#endif
#ifdef ENABLE_AUDIOFILE
	&audiofile_decoder_plugin,
#endif
#ifdef ENABLE_DSD
	&dsdiff_decoder_plugin,
	&dsf_decoder_plugin,
	&hybrid_dsd_decoder_plugin,
#endif
#ifdef ENABLE_FAAD
	&faad_decoder_plugin,
#endif
#ifdef ENABLE_MPCDEC
	&mpcdec_decoder_plugin,
#endif
#ifdef ENABLE_WAVPACK
	&wavpack_decoder_plugin,
#endif
#ifdef ENABLE_MODPLUG
	&modplug_decoder_plugin,
#endif
#ifdef ENABLE_LIBMIKMOD
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
#ifdef ENABLE_ADPLUG
	&adplug_decoder_plugin,
#endif
#ifdef ENABLE_FFMPEG
	&ffmpeg_decoder_plugin,
#endif
#ifdef ENABLE_GME
	&gme_decoder_plugin,
#endif
	&pcm_decoder_plugin,
	nullptr
};

static constexpr unsigned num_decoder_plugins =
	std::size(decoder_plugins) - 1;

/** which plugins have been initialized successfully? */
bool decoder_plugins_enabled[num_decoder_plugins];

const struct DecoderPlugin *
decoder_plugin_from_name(const char *name) noexcept
{
	return decoder_plugins_find([=](const DecoderPlugin &plugin){
			return strcmp(plugin.name, name) == 0;
		});
}

void
decoder_plugin_init_all(const ConfigData &config)
{
	ConfigBlock empty;

	for (unsigned i = 0; decoder_plugins[i] != nullptr; ++i) {
		const DecoderPlugin &plugin = *decoder_plugins[i];
		const auto *param =
			config.FindBlock(ConfigBlockOption::DECODER, "plugin",
					 plugin.name);

		if (param == nullptr)
			param = &empty;
		else if (!param->GetBlockValue("enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		if (param != nullptr)
			param->SetUsed();

		try {
			if (plugin.Init(*param))
				decoder_plugins_enabled[i] = true;
		} catch (const PluginUnavailable &e) {
			FormatError(e,
				    "Decoder plugin '%s' is unavailable",
				    plugin.name);
		} catch (...) {
			std::throw_with_nested(FormatRuntimeError("Failed to initialize decoder plugin '%s'",
								  plugin.name));
		}
	}
}

void
decoder_plugin_deinit_all() noexcept
{
	decoder_plugins_for_each_enabled([=](const DecoderPlugin &plugin){
			plugin.Finish();
		});
}

bool
decoder_plugins_supports_suffix(const char *suffix) noexcept
{
	return decoder_plugins_try([suffix](const DecoderPlugin &plugin){
			return plugin.SupportsSuffix(suffix);
		});
}
