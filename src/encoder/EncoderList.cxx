/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

constexpr const EncoderPlugin *encoder_plugins[] = {
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
