// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "EncoderList.hxx"
#include "EncoderPlugin.hxx"
#include "encoder/Features.h"
#include "plugins/NullEncoderPlugin.hxx"
#include "plugins/WaveEncoderPlugin.hxx"
#include "plugins/VorbisEncoderPlugin.hxx"
#include "plugins/OpusEncoderPlugin.hxx"
#include "plugins/FlacEncoderPlugin.hxx"
#include "plugins/ShineEncoderPlugin.hxx"
#include "plugins/LameEncoderPlugin.hxx"
#include "plugins/TwolameEncoderPlugin.hxx"
#include "decoder/Features.h"

#include <string.h>

constinit const EncoderPlugin *const encoder_plugins[] = {
	&null_encoder_plugin,
#ifdef ENABLE_VORBISENC
	&vorbis_encoder_plugin,
#endif
#ifdef ENABLE_OPUS
	&opus_encoder_plugin,
#endif
#ifdef ENABLE_LAME
	&lame_encoder_plugin,
#endif
#ifdef ENABLE_TWOLAME
	&twolame_encoder_plugin,
#endif
#ifdef ENABLE_WAVE_ENCODER
	&wave_encoder_plugin,
#endif
#ifdef ENABLE_FLAC_ENCODER
	&flac_encoder_plugin,
#endif
#ifdef ENABLE_SHINE
	&shine_encoder_plugin,
#endif
	nullptr
};

const EncoderPlugin *
encoder_plugin_get(const char *name)
{
	encoder_plugins_for_each(plugin)
		if (strcmp(plugin->name, name) == 0)
			return plugin;

	return nullptr;
}
