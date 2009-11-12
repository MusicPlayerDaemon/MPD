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
#include "mixer_api.h"
#include "output_api.h"

#include <glib.h>
#include <alsa/asoundlib.h>

#define VOLUME_MIXER_ALSA_DEFAULT		"default"
#define VOLUME_MIXER_ALSA_CONTROL_DEFAULT	"PCM"
#define VOLUME_MIXER_ALSA_INDEX_DEFAULT		0

struct alsa_mixer {
	/** the base mixer class */
	struct mixer base;

	const char *device;
	const char *control;
	unsigned int index;

	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	long volume_min;
	long volume_max;
	int volume_set;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
alsa_mixer_quark(void)
{
	return g_quark_from_static_string("alsa_mixer");
}

static struct mixer *
alsa_mixer_init(G_GNUC_UNUSED void *ao, const struct config_param *param,
		G_GNUC_UNUSED GError **error_r)
{
	struct alsa_mixer *am = g_new(struct alsa_mixer, 1);

	mixer_init(&am->base, &alsa_mixer_plugin);

	am->device = config_get_block_string(param, "mixer_device",
					     VOLUME_MIXER_ALSA_DEFAULT);
	am->control = config_get_block_string(param, "mixer_control",
					      VOLUME_MIXER_ALSA_CONTROL_DEFAULT);
	am->index = config_get_block_unsigned(param, "mixer_index",
					      VOLUME_MIXER_ALSA_INDEX_DEFAULT);

	return &am->base;
}

static void
alsa_mixer_finish(struct mixer *data)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;

	g_free(am);

	/* free libasound's config cache */
	snd_config_update_free_global();
}

static void
alsa_mixer_close(struct mixer *data)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;

	assert(am->handle != NULL);

	snd_mixer_close(am->handle);
}

static bool
alsa_mixer_open(struct mixer *data, GError **error_r)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;
	int err;
	snd_mixer_elem_t *elem;

	am->volume_set = -1;

	err = snd_mixer_open(&am->handle, 0);
	if (err < 0) {
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "snd_mixer_open() failed: %s", snd_strerror(err));
		return false;
	}

	if ((err = snd_mixer_attach(am->handle, am->device)) < 0) {
		alsa_mixer_close(data);
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "failed to attach to %s: %s",
			    am->device, snd_strerror(err));
		return false;
	}

	if ((err = snd_mixer_selem_register(am->handle, NULL,
		    NULL)) < 0) {
		alsa_mixer_close(data);
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "snd_mixer_selem_register() failed: %s",
			    snd_strerror(err));
		return false;
	}

	if ((err = snd_mixer_load(am->handle)) < 0) {
		alsa_mixer_close(data);
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "snd_mixer_load() failed: %s\n",
			    snd_strerror(err));
		return false;
	}

	elem = snd_mixer_first_elem(am->handle);

	while (elem) {
		if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE) {
			if ((g_ascii_strcasecmp(am->control,
						snd_mixer_selem_get_name(elem)) == 0) &&
			    (am->index == snd_mixer_selem_get_index(elem))) {
				break;
			}
		}
		elem = snd_mixer_elem_next(elem);
	}

	if (elem) {
		am->elem = elem;
		snd_mixer_selem_get_playback_volume_range(am->elem,
							  &am->volume_min,
							  &am->volume_max);
		return true;
	}

	alsa_mixer_close(data);
	g_set_error(error_r, alsa_mixer_quark(), 0,
		    "no such mixer control: %s", am->control);
	return false;
}

static int
alsa_mixer_get_volume(struct mixer *mixer, GError **error_r)
{
	struct alsa_mixer *am = (struct alsa_mixer *)mixer;
	int err;
	int ret;
	long level;

	assert(am->handle != NULL);

	err = snd_mixer_handle_events(am->handle);
	if (err < 0) {
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "snd_mixer_handle_events() failed: %s",
			    snd_strerror(err));
		return false;
	}

	err = snd_mixer_selem_get_playback_volume(am->elem,
						  SND_MIXER_SCHN_FRONT_LEFT,
						  &level);
	if (err < 0) {
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "failed to read ALSA volume: %s",
			    snd_strerror(err));
		return false;
	}

	ret = ((am->volume_set / 100.0) * (am->volume_max - am->volume_min)
	       + am->volume_min) + 0.5;
	if (am->volume_set > 0 && ret == level) {
		ret = am->volume_set;
	} else {
		ret = (int)(100 * (((float)(level - am->volume_min)) /
				   (am->volume_max - am->volume_min)) + 0.5);
	}

	return ret;
}

static bool
alsa_mixer_set_volume(struct mixer *mixer, unsigned volume, GError **error_r)
{
	struct alsa_mixer *am = (struct alsa_mixer *)mixer;
	float vol;
	long level;
	int err;

	assert(am->handle != NULL);

	vol = volume;

	am->volume_set = vol + 0.5;

	level = (long)(((vol / 100.0) * (am->volume_max - am->volume_min) +
			am->volume_min) + 0.5);
	level = level > am->volume_max ? am->volume_max : level;
	level = level < am->volume_min ? am->volume_min : level;

	err = snd_mixer_selem_set_playback_volume_all(am->elem, level);
	if (err < 0) {
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "failed to set ALSA volume: %s",
			    snd_strerror(err));
		return false;
	}

	return true;
}

const struct mixer_plugin alsa_mixer_plugin = {
	.init = alsa_mixer_init,
	.finish = alsa_mixer_finish,
	.open = alsa_mixer_open,
	.close = alsa_mixer_close,
	.get_volume = alsa_mixer_get_volume,
	.set_volume = alsa_mixer_set_volume,
	.global = true,
};
