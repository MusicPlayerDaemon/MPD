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
#include "software_mixer_plugin.h"
#include "mixer_api.h"
#include "filter_plugin.h"
#include "filter_registry.h"
#include "filter/volume_filter_plugin.h"
#include "pcm_volume.h"

#include <assert.h>
#include <math.h>

struct software_mixer {
	/** the base mixer class */
	struct mixer base;

	struct filter *filter;

	unsigned volume;
};

static struct mixer *
software_mixer_init(G_GNUC_UNUSED void *ao,
		    G_GNUC_UNUSED const struct config_param *param,
		    G_GNUC_UNUSED GError **error_r)
{
	struct software_mixer *sm = g_new(struct software_mixer, 1);

	mixer_init(&sm->base, &software_mixer_plugin);

	sm->filter = filter_new(&volume_filter_plugin, NULL, NULL);
	assert(sm->filter != NULL);

	sm->volume = 100;

	return &sm->base;
}

static void
software_mixer_finish(struct mixer *data)
{
	struct software_mixer *sm = (struct software_mixer *)data;

	g_free(sm);
}

static int
software_mixer_get_volume(struct mixer *mixer, G_GNUC_UNUSED GError **error_r)
{
	struct software_mixer *sm = (struct software_mixer *)mixer;

	return sm->volume;
}

static bool
software_mixer_set_volume(struct mixer *mixer, unsigned volume,
			  G_GNUC_UNUSED GError **error_r)
{
	struct software_mixer *sm = (struct software_mixer *)mixer;

	assert(volume <= 100);

	sm->volume = volume;

	if (volume >= 100)
		volume = PCM_VOLUME_1;
	else if (volume > 0)
		volume = pcm_float_to_volume((exp(volume / 25.0) - 1) /
					     (54.5981500331F - 1));

	volume_filter_set(sm->filter, volume);
	return true;
}

const struct mixer_plugin software_mixer_plugin = {
	.init = software_mixer_init,
	.finish = software_mixer_finish,
	.get_volume = software_mixer_get_volume,
	.set_volume = software_mixer_set_volume,
	.global = true,
};

struct filter *
software_mixer_get_filter(struct mixer *mixer)
{
	struct software_mixer *sm = (struct software_mixer *)mixer;

	assert(sm->base.plugin == &software_mixer_plugin);

	return sm->filter;
}
