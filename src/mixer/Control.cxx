/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "Control.hxx"
#include "Mixer.hxx"

#include <cassert>

Mixer *
mixer_new(EventLoop &event_loop,
	  const MixerPlugin &plugin, AudioOutput &ao,
	  MixerListener &listener,
	  const ConfigBlock &block)
{
	Mixer *mixer = plugin.init(event_loop, ao, listener, block);

	assert(mixer == nullptr || mixer->IsPlugin(plugin));

	return mixer;
}

void
mixer_free(Mixer *mixer) noexcept
{
	assert(mixer != nullptr);

	/* mixers with the "global" flag set might still be open at
	   this point (see mixer_auto_close()) */
	mixer_close(*mixer);

	delete mixer;
}

void
mixer_open(Mixer &mixer)
{
	const std::scoped_lock<Mutex> protect(mixer.mutex);

	if (mixer.open)
		return;

	try {
		mixer.Open();
		mixer.open = true;
		mixer.failure = {};
	} catch (...) {
		mixer.failure = std::current_exception();
		throw;
	}
}

static void
mixer_close_internal(Mixer &mixer) noexcept
{
	assert(mixer.open);

	mixer.Close();
	mixer.open = false;
	mixer.failure = {};
}

void
mixer_close(Mixer &mixer) noexcept
{
	const std::scoped_lock<Mutex> protect(mixer.mutex);

	if (mixer.open)
		mixer_close_internal(mixer);
}

void
mixer_auto_close(Mixer &mixer) noexcept
{
	if (!mixer.IsGlobal())
		mixer_close(mixer);
}

int
mixer_get_volume(Mixer &mixer)
{
	int volume;

	if (mixer.IsGlobal() && !mixer.failure)
		mixer_open(mixer);

	const std::scoped_lock<Mutex> protect(mixer.mutex);

	if (mixer.open) {
		try {
			volume = mixer.GetVolume();
		} catch (...) {
			mixer_close_internal(mixer);
			mixer.failure = std::current_exception();
			throw;
		}
	} else
		volume = -1;

	return volume;
}

void
mixer_set_volume(Mixer &mixer, unsigned volume)
{
	assert(volume <= 100);

	if (mixer.IsGlobal() && !mixer.failure)
		mixer_open(mixer);

	const std::scoped_lock<Mutex> protect(mixer.mutex);

	if (mixer.open)
		mixer.SetVolume(volume);
	else if (mixer.failure)
		std::rethrow_exception(mixer.failure);
}
