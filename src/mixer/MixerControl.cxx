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
#include "MixerControl.hxx"
#include "MixerInternal.hxx"
#include "util/Error.hxx"

#include <assert.h>

Mixer *
mixer_new(EventLoop &event_loop,
	  const MixerPlugin &plugin, AudioOutput &ao,
	  MixerListener &listener,
	  const config_param &param,
	  Error &error)
{
	Mixer *mixer = plugin.init(event_loop, ao, listener, param, error);

	assert(mixer == nullptr || mixer->IsPlugin(plugin));

	return mixer;
}

void
mixer_free(Mixer *mixer)
{
	assert(mixer != nullptr);

	/* mixers with the "global" flag set might still be open at
	   this point (see mixer_auto_close()) */
	mixer_close(mixer);

	delete mixer;
}

bool
mixer_open(Mixer *mixer, Error &error)
{
	bool success;

	assert(mixer != nullptr);

	const ScopeLock protect(mixer->mutex);

	success = mixer->open || (mixer->open = mixer->Open(error));

	mixer->failed = !success;

	return success;
}

static void
mixer_close_internal(Mixer *mixer)
{
	assert(mixer != nullptr);
	assert(mixer->open);

	mixer->Close();
	mixer->open = false;
}

void
mixer_close(Mixer *mixer)
{
	assert(mixer != nullptr);

	const ScopeLock protect(mixer->mutex);

	if (mixer->open)
		mixer_close_internal(mixer);
}

void
mixer_auto_close(Mixer *mixer)
{
	if (!mixer->plugin.global)
		mixer_close(mixer);
}

/*
 * Close the mixer due to failure.  The mutex must be locked before
 * calling this function.
 */
static void
mixer_failed(Mixer *mixer)
{
	assert(mixer->open);

	mixer_close_internal(mixer);

	mixer->failed = true;
}

int
mixer_get_volume(Mixer *mixer, Error &error)
{
	int volume;

	assert(mixer != nullptr);

	if (mixer->plugin.global && !mixer->failed &&
	    !mixer_open(mixer, error))
		return -1;

	const ScopeLock protect(mixer->mutex);

	if (mixer->open) {
		volume = mixer->GetVolume(error);
		if (volume < 0 && error.IsDefined())
			mixer_failed(mixer);
	} else
		volume = -1;

	return volume;
}

bool
mixer_set_volume(Mixer *mixer, unsigned volume, Error &error)
{
	assert(mixer != nullptr);
	assert(volume <= 100);

	if (mixer->plugin.global && !mixer->failed &&
	    !mixer_open(mixer, error))
		return false;

	const ScopeLock protect(mixer->mutex);

	return mixer->open && mixer->SetVolume(volume, error);
}
