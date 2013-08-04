/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include <glib.h>

#include <assert.h>
#include <stddef.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mixer"

Mixer *
mixer_new(const struct mixer_plugin *plugin, void *ao,
	  const config_param &param,
	  GError **error_r)
{
	Mixer *mixer;

	assert(plugin != NULL);

	mixer = plugin->init(ao, param, error_r);

	assert(mixer == NULL || mixer->IsPlugin(*plugin));

	return mixer;
}

void
mixer_free(Mixer *mixer)
{
	assert(mixer != NULL);
	assert(mixer->plugin != NULL);

	/* mixers with the "global" flag set might still be open at
	   this point (see mixer_auto_close()) */
	mixer_close(mixer);

	mixer->plugin->finish(mixer);
}

bool
mixer_open(Mixer *mixer, GError **error_r)
{
	bool success;

	assert(mixer != NULL);
	assert(mixer->plugin != NULL);

	const ScopeLock protect(mixer->mutex);

	if (mixer->open)
		success = true;
	else if (mixer->plugin->open == NULL)
		success = mixer->open = true;
	else
		success = mixer->open = mixer->plugin->open(mixer, error_r);

	mixer->failed = !success;

	return success;
}

static void
mixer_close_internal(Mixer *mixer)
{
	assert(mixer != NULL);
	assert(mixer->plugin != NULL);
	assert(mixer->open);

	if (mixer->plugin->close != NULL)
		mixer->plugin->close(mixer);

	mixer->open = false;
}

void
mixer_close(Mixer *mixer)
{
	assert(mixer != NULL);
	assert(mixer->plugin != NULL);

	const ScopeLock protect(mixer->mutex);

	if (mixer->open)
		mixer_close_internal(mixer);
}

void
mixer_auto_close(Mixer *mixer)
{
	if (!mixer->plugin->global)
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
mixer_get_volume(Mixer *mixer, GError **error_r)
{
	int volume;

	assert(mixer != NULL);

	if (mixer->plugin->global && !mixer->failed &&
	    !mixer_open(mixer, error_r))
		return -1;

	const ScopeLock protect(mixer->mutex);

	if (mixer->open) {
		GError *error = NULL;

		volume = mixer->plugin->get_volume(mixer, &error);
		if (volume < 0 && error != NULL) {
			g_propagate_error(error_r, error);
			mixer_failed(mixer);
		}
	} else
		volume = -1;

	return volume;
}

bool
mixer_set_volume(Mixer *mixer, unsigned volume, GError **error_r)
{
	assert(mixer != NULL);
	assert(volume <= 100);

	if (mixer->plugin->global && !mixer->failed &&
	    !mixer_open(mixer, error_r))
		return false;

	const ScopeLock protect(mixer->mutex);

	return mixer->open &&
		mixer->plugin->set_volume(mixer, volume, error_r);
}
