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
#include "mixer_control.h"
#include "mixer_api.h"

#include <assert.h>
#include <stddef.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mixer"

struct mixer *
mixer_new(const struct mixer_plugin *plugin, void *ao,
	  const struct config_param *param,
	  GError **error_r)
{
	struct mixer *mixer;

	assert(plugin != NULL);

	mixer = plugin->init(ao, param, error_r);

	assert(mixer == NULL || mixer->plugin == plugin);

	return mixer;
}

void
mixer_free(struct mixer *mixer)
{
	assert(mixer != NULL);
	assert(mixer->plugin != NULL);
	assert(mixer->mutex != NULL);

	g_mutex_free(mixer->mutex);

	mixer->plugin->finish(mixer);
}

bool
mixer_open(struct mixer *mixer, GError **error_r)
{
	bool success;

	assert(mixer != NULL);
	assert(mixer->plugin != NULL);

	g_mutex_lock(mixer->mutex);

	if (mixer->open)
		success = true;
	else if (mixer->plugin->open == NULL)
		success = mixer->open = true;
	else
		success = mixer->open = mixer->plugin->open(mixer, error_r);

	mixer->failed = !success;

	g_mutex_unlock(mixer->mutex);

	return success;
}

static void
mixer_close_internal(struct mixer *mixer)
{
	assert(mixer != NULL);
	assert(mixer->plugin != NULL);
	assert(mixer->open);

	if (mixer->plugin->close != NULL)
		mixer->plugin->close(mixer);

	mixer->open = false;
}

void
mixer_close(struct mixer *mixer)
{
	assert(mixer != NULL);
	assert(mixer->plugin != NULL);

	g_mutex_lock(mixer->mutex);

	if (mixer->open)
		mixer_close_internal(mixer);

	g_mutex_unlock(mixer->mutex);
}

void
mixer_auto_close(struct mixer *mixer)
{
	if (!mixer->plugin->global)
		mixer_close(mixer);
}

/*
 * Close the mixer due to failure.  The mutex must be locked before
 * calling this function.
 */
static void
mixer_failed(struct mixer *mixer)
{
	assert(mixer->open);

	mixer_close_internal(mixer);

	mixer->failed = true;
}

int
mixer_get_volume(struct mixer *mixer, GError **error_r)
{
	int volume;

	assert(mixer != NULL);

	if (mixer->plugin->global && !mixer->failed &&
	    !mixer_open(mixer, error_r))
		return -1;

	g_mutex_lock(mixer->mutex);

	if (mixer->open) {
		GError *error = NULL;

		volume = mixer->plugin->get_volume(mixer, &error);
		if (volume < 0 && error != NULL) {
			g_propagate_error(error_r, error);
			mixer_failed(mixer);
		}
	} else
		volume = -1;

	g_mutex_unlock(mixer->mutex);

	return volume;
}

bool
mixer_set_volume(struct mixer *mixer, unsigned volume, GError **error_r)
{
	bool success;

	assert(mixer != NULL);
	assert(volume <= 100);

	if (mixer->plugin->global && !mixer->failed &&
	    !mixer_open(mixer, error_r))
		return false;

	g_mutex_lock(mixer->mutex);

	if (mixer->open) {
		success = mixer->plugin->set_volume(mixer, volume, error_r);
	} else
		success = false;

	g_mutex_unlock(mixer->mutex);

	return success;
}
