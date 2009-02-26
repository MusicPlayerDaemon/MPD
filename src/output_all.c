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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "output_all.h"
#include "output_internal.h"
#include "output_control.h"
#include "conf.h"

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "output"

static struct audio_format input_audio_format;

static struct audio_output *audio_outputs;
static unsigned int num_audio_outputs;

unsigned int audio_output_count(void)
{
	return num_audio_outputs;
}

struct audio_output *
audio_output_get(unsigned i)
{
	assert(i < num_audio_outputs);

	return &audio_outputs[i];
}

struct audio_output *
audio_output_find(const char *name)
{
	for (unsigned i = 0; i < num_audio_outputs; ++i) {
		struct audio_output *ao = audio_output_get(i);

		if (strcmp(ao->name, name) == 0)
			return ao;
	}

	/* name not found */
	return NULL;
}

static unsigned
audio_output_config_count(void)
{
	unsigned int nr = 0;
	const struct config_param *param = NULL;

	while ((param = config_get_next_param(CONF_AUDIO_OUTPUT, param)))
		nr++;
	if (!nr)
		nr = 1; /* we'll always have at least one device  */
	return nr;
}

void
audio_output_all_init(void)
{
	const struct config_param *param = NULL;
	unsigned int i;

	notify_init(&audio_output_client_notify);

	num_audio_outputs = audio_output_config_count();
	audio_outputs = g_new(struct audio_output, num_audio_outputs);

	for (i = 0; i < num_audio_outputs; i++)
	{
		struct audio_output *output = &audio_outputs[i];
		unsigned int j;

		param = config_get_next_param(CONF_AUDIO_OUTPUT, param);

		/* only allow param to be NULL if there just one audioOutput */
		assert(param || (num_audio_outputs == 1));

		if (!audio_output_init(output, param)) {
			if (param)
			{
				g_error("problems configuring output device "
					"defined at line %i\n", param->line);
			}
			else
			{
				g_error("No audio_output specified and unable to "
					"detect a default audio output device\n");
			}
		}

		/* require output names to be unique: */
		for (j = 0; j < i; j++) {
			if (!strcmp(output->name, audio_outputs[j].name)) {
				g_error("output devices with identical "
					"names: %s\n", output->name);
			}
		}
	}
}

void
audio_output_all_finish(void)
{
	unsigned int i;

	for (i = 0; i < num_audio_outputs; i++) {
		audio_output_finish(&audio_outputs[i]);
	}

	g_free(audio_outputs);
	audio_outputs = NULL;
	num_audio_outputs = 0;

	notify_deinit(&audio_output_client_notify);
}


/**
 * Determine if all (active) outputs have finished the current
 * command.
 */
static bool
audio_output_all_finished(void)
{
	for (unsigned i = 0; i < num_audio_outputs; ++i)
		if (audio_output_is_open(&audio_outputs[i]) &&
		    !audio_output_command_is_finished(&audio_outputs[i]))
			return false;

	return true;
}

static void audio_output_wait_all(void)
{
	while (!audio_output_all_finished())
		notify_wait(&audio_output_client_notify);
}

/**
 * Resets the "reopen" flag on all audio devices.  MPD should
 * immediately retry to open the device instead of waiting for the
 * timeout when the user wants to start playback.
 */
static void
audio_output_all_reset_reopen(void)
{
	for (unsigned i = 0; i < num_audio_outputs; ++i)
		audio_outputs[i].reopen_after = 0;
}

static void
audio_output_all_update(void)
{
	unsigned int i;

	if (!audio_format_defined(&input_audio_format))
		return;

	for (i = 0; i < num_audio_outputs; ++i)
		audio_output_update(&audio_outputs[i], &input_audio_format);
}

bool
audio_output_all_play(const char *buffer, size_t length)
{
	bool ret = false;
	unsigned int i;

	assert(length > 0);
	/* no partial frames allowed */
	assert((length % audio_format_frame_size(&input_audio_format)) == 0);

	audio_output_all_update();

	for (i = 0; i < num_audio_outputs; ++i)
		if (audio_output_is_open(&audio_outputs[i]))
			audio_output_play(&audio_outputs[i],
					  buffer, length);

	while (true) {
		bool finished = true;

		for (i = 0; i < num_audio_outputs; ++i) {
			struct audio_output *ao = &audio_outputs[i];

			if (!audio_output_is_open(ao))
				continue;

			if (audio_output_command_is_finished(ao))
				ret = true;
			else {
				finished = false;
				audio_output_signal(ao);
			}
		}

		if (finished)
			break;

		notify_wait(&audio_output_client_notify);
	};

	return ret;
}

bool
audio_output_all_open(const struct audio_format *audio_format)
{
	bool ret = false;
	unsigned int i;

	if (audio_format != NULL)
		input_audio_format = *audio_format;

	audio_output_all_reset_reopen();
	audio_output_all_update();

	for (i = 0; i < num_audio_outputs; ++i) {
		if (audio_outputs[i].open)
			ret = true;
	}

	if (!ret)
		/* close all devices if there was an error */
		audio_output_all_close();

	return ret;
}

void
audio_output_all_pause(void)
{
	unsigned int i;

	audio_output_all_update();

	for (i = 0; i < num_audio_outputs; ++i)
		if (audio_output_is_open(&audio_outputs[i]))
			audio_output_pause(&audio_outputs[i]);

	audio_output_wait_all();
}

void
audio_output_all_cancel(void)
{
	unsigned int i;

	audio_output_all_update();

	for (i = 0; i < num_audio_outputs; ++i) {
		if (audio_output_is_open(&audio_outputs[i]))
			audio_output_cancel(&audio_outputs[i]);
	}

	audio_output_wait_all();
}

void
audio_output_all_close(void)
{
	unsigned int i;

	for (i = 0; i < num_audio_outputs; ++i)
		audio_output_close(&audio_outputs[i]);
}

void
audio_output_all_tag(const struct tag *tag)
{
	unsigned int i;

	for (i = 0; i < num_audio_outputs; ++i)
		if (audio_output_is_open(&audio_outputs[i]))
			audio_output_send_tag(&audio_outputs[i], tag);

	audio_output_wait_all();
}
