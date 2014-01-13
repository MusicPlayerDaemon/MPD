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
#include "OutputPlugin.hxx"
#include "OutputInternal.hxx"

struct audio_output *
ao_plugin_init(const struct audio_output_plugin *plugin,
	       const config_param &param,
	       Error &error)
{
	assert(plugin != nullptr);
	assert(plugin->init != nullptr);

	return plugin->init(param, error);
}

void
ao_plugin_finish(struct audio_output *ao)
{
	ao->plugin->finish(ao);
}

bool
ao_plugin_enable(struct audio_output *ao, Error &error_r)
{
	return ao->plugin->enable != nullptr
		? ao->plugin->enable(ao, error_r)
		: true;
}

void
ao_plugin_disable(struct audio_output *ao)
{
	if (ao->plugin->disable != nullptr)
		ao->plugin->disable(ao);
}

bool
ao_plugin_open(struct audio_output *ao, AudioFormat &audio_format,
	       Error &error)
{
	return ao->plugin->open(ao, audio_format, error);
}

void
ao_plugin_close(struct audio_output *ao)
{
	ao->plugin->close(ao);
}

unsigned
ao_plugin_delay(struct audio_output *ao)
{
	return ao->plugin->delay != nullptr
		? ao->plugin->delay(ao)
		: 0;
}

void
ao_plugin_send_tag(struct audio_output *ao, const Tag *tag)
{
	if (ao->plugin->send_tag != nullptr)
		ao->plugin->send_tag(ao, tag);
}

size_t
ao_plugin_play(struct audio_output *ao, const void *chunk, size_t size,
	       Error &error)
{
	return ao->plugin->play(ao, chunk, size, error);
}

void
ao_plugin_drain(struct audio_output *ao)
{
	if (ao->plugin->drain != nullptr)
		ao->plugin->drain(ao);
}

void
ao_plugin_cancel(struct audio_output *ao)
{
	if (ao->plugin->cancel != nullptr)
		ao->plugin->cancel(ao);
}

bool
ao_plugin_pause(struct audio_output *ao)
{
	return ao->plugin->pause != nullptr && ao->plugin->pause(ao);
}
