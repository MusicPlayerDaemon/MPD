// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "DecoderList.hxx"
#include "DecoderPlugin.hxx"
#include "Domain.hxx"
#include "decoder/Features.h"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "config/Data.hxx"
#include "config/Block.hxx"
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
#include "plugins/OpenmptDecoderPlugin.hxx"
#include "plugins/MpcdecDecoderPlugin.hxx"
#include "plugins/FluidsynthDecoderPlugin.hxx"
#include "plugins/SidplayDecoderPlugin.hxx"
#include "Log.hxx"
#include "PluginUnavailable.hxx"

#include <iterator>

#include <string.h>

constinit const struct DecoderPlugin *const decoder_plugins[] = {
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
#ifdef ENABLE_OPENMPT
	&openmpt_decoder_plugin,
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
#ifdef ENABLE_GME
	&gme_decoder_plugin,
#endif
#ifdef ENABLE_FFMPEG
	&ffmpeg_decoder_plugin,
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
			FmtError(decoder_domain,
				 "Decoder plugin '{}' is unavailable: {}",
				 plugin.name, std::current_exception());
		} catch (...) {
			std::throw_with_nested(FmtRuntimeError("Failed to initialize decoder plugin '{}'",
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
decoder_plugins_supports_suffix(std::string_view suffix) noexcept
{
	return decoder_plugins_try([suffix](const DecoderPlugin &plugin){
			return plugin.SupportsSuffix(suffix);
		});
}
