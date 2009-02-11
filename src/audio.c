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

#include "audio.h"
#include "audio_format.h"
#include "audio_parser.h"
#include "output_api.h"
#include "output_control.h"
#include "output_internal.h"
#include "output_all.h"
#include "path.h"
#include "idle.h"
#include "mixer_api.h"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>

static struct audio_format configured_audio_format;

void getOutputAudioFormat(const struct audio_format *inAudioFormat,
			  struct audio_format *outAudioFormat)
{
	*outAudioFormat = audio_format_defined(&configured_audio_format)
		? configured_audio_format
		: *inAudioFormat;
}

void initAudioConfig(void)
{
	const struct config_param *param = config_get_param(CONF_AUDIO_OUTPUT_FORMAT);
	GError *error = NULL;
	bool ret;

	if (NULL == param || NULL == param->value)
		return;

	ret = audio_format_parse(&configured_audio_format, param->value,
				 &error);
	if (!ret)
		g_error("error parsing \"%s\" at line %i: %s",
			CONF_AUDIO_OUTPUT_FORMAT, param->line, error->message);
}

void finishAudioConfig(void)
{
	audio_format_clear(&configured_audio_format);
}

int enableAudioDevice(unsigned int device)
{
	struct audio_output *ao;

	if (device >= audio_output_count())
		return -1;

	ao = audio_output_get(device);

	ao->reopen_after = 0;
	ao->enabled = true;
	idle_add(IDLE_OUTPUT);

	return 0;
}

int disableAudioDevice(unsigned int device)
{
	struct audio_output *ao;

	if (device >= audio_output_count())
		return -1;

	ao = audio_output_get(device);

	ao->enabled = false;
	idle_add(IDLE_OUTPUT);

	return 0;
}

bool mixer_control_setvol(unsigned int device, int volume, int rel)
{
	struct audio_output *output;

	if (device >= audio_output_count())
		return false;

	output = audio_output_get(device);
	if (output->plugin && output->plugin->control) {
		if (rel) {
			int cur_volume;
			if (!output->plugin->control(output->data, AC_MIXER_GETVOL, &cur_volume)) {
				return false;
			}
			volume = volume + cur_volume;
		}
		if (volume > 100)
			volume = 100;
		else if (volume < 0)
			volume = 0;

		return output->plugin->control(output->data, AC_MIXER_SETVOL, &volume);
	}
	return false;
}

bool mixer_control_getvol(unsigned int device, int *volume)
{
	struct audio_output *output;

	if (device >= audio_output_count())
		return false;

	output = audio_output_get(device);
	if (output->plugin && output->plugin->control) {
		return output->plugin->control(output->data, AC_MIXER_GETVOL, volume);
	}
	return false;
}
