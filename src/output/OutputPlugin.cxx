/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "Internal.hxx"

FilteredAudioOutput *
ao_plugin_init(EventLoop &event_loop,
	       const AudioOutputPlugin &plugin,
	       const ConfigBlock &block)
{
	assert(plugin.init != nullptr);

	return plugin.init(event_loop, block);
}

void
ao_plugin_finish(FilteredAudioOutput *ao) noexcept
{
	ao->plugin.finish(ao);
}

void
ao_plugin_enable(FilteredAudioOutput &ao)
{
	if (ao.plugin.enable != nullptr)
		ao.plugin.enable(&ao);
}

void
ao_plugin_disable(FilteredAudioOutput &ao) noexcept
{
	if (ao.plugin.disable != nullptr)
		ao.plugin.disable(&ao);
}

void
ao_plugin_open(FilteredAudioOutput &ao, AudioFormat &audio_format)
{
	ao.plugin.open(&ao, audio_format);
}

void
ao_plugin_close(FilteredAudioOutput &ao) noexcept
{
	ao.plugin.close(&ao);
}

std::chrono::steady_clock::duration
ao_plugin_delay(FilteredAudioOutput &ao) noexcept
{
	return ao.plugin.delay != nullptr
		? ao.plugin.delay(&ao)
		: std::chrono::steady_clock::duration::zero();
}

void
ao_plugin_send_tag(FilteredAudioOutput &ao, const Tag &tag)
{
	if (ao.plugin.send_tag != nullptr)
		ao.plugin.send_tag(&ao, tag);
}

size_t
ao_plugin_play(FilteredAudioOutput &ao, const void *chunk, size_t size)
{
	return ao.plugin.play(&ao, chunk, size);
}

void
ao_plugin_drain(FilteredAudioOutput &ao)
{
	if (ao.plugin.drain != nullptr)
		ao.plugin.drain(&ao);
}

void
ao_plugin_cancel(FilteredAudioOutput &ao) noexcept
{
	if (ao.plugin.cancel != nullptr)
		ao.plugin.cancel(&ao);
}

bool
ao_plugin_pause(FilteredAudioOutput &ao)
{
	return ao.plugin.pause != nullptr && ao.plugin.pause(&ao);
}
