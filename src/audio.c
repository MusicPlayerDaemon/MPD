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
#include "audio.h"
#include "audio_format.h"
#include "audio_parser.h"
#include "output_internal.h"
#include "output_plugin.h"
#include "output_all.h"
#include "conf.h"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>

static struct audio_format configured_audio_format;

void getOutputAudioFormat(const struct audio_format *inAudioFormat,
			  struct audio_format *outAudioFormat)
{
	*outAudioFormat = *inAudioFormat;
	audio_format_mask_apply(outAudioFormat, &configured_audio_format);
}

void initAudioConfig(void)
{
	const struct config_param *param = config_get_param(CONF_AUDIO_OUTPUT_FORMAT);
	GError *error = NULL;
	bool ret;

	if (param == NULL)
		return;

	ret = audio_format_parse(&configured_audio_format, param->value,
				 true, &error);
	if (!ret)
		g_error("error parsing \"%s\" at line %i: %s",
			CONF_AUDIO_OUTPUT_FORMAT, param->line, error->message);
}
