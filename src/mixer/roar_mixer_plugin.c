/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
 * Copyright (C) 2010-2011 Philipp 'ph3-der-loewe' Schafft
 * Copyright (C) 2010-2011 Hans-Kristian 'maister' Arntzen
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
#include "output/roar_output_plugin.h"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct roar_mpd_mixer
{
	/** the base mixer class */
	struct mixer base;
	struct roar *self;
} roar_mixer_t;

/**
 * The quark used for GError.domain.
 */
static inline GQuark
roar_mixer_quark(void)
{
	return g_quark_from_static_string("roar_mixer");
}

static struct mixer *
roar_mixer_init(void *ao, G_GNUC_UNUSED const struct config_param *param,
		G_GNUC_UNUSED GError **error_r)
{
	roar_mixer_t *self = g_new(roar_mixer_t, 1);
	self->self = ao;

	mixer_init(&self->base, &roar_mixer_plugin);

	return &self->base;
}

static void
roar_mixer_finish(struct mixer *data)
{
	roar_mixer_t *self = (roar_mixer_t *) data;

	g_free(self);
}

static void
roar_mixer_close(G_GNUC_UNUSED struct mixer *data)
{
}

static bool
roar_mixer_open(G_GNUC_UNUSED struct mixer *data,
		G_GNUC_UNUSED GError **error_r)
{
	return true;
}

static int
roar_mixer_get_volume(struct mixer *mixer, G_GNUC_UNUSED GError **error_r)
{
	roar_mixer_t *self = (roar_mixer_t *)mixer;
	return roar_output_get_volume(self->self);
}

static bool
roar_mixer_set_volume(struct mixer *mixer, unsigned volume,
		G_GNUC_UNUSED GError **error_r)
{
	roar_mixer_t *self = (roar_mixer_t *)mixer;
	return roar_output_set_volume(self->self, volume);
}

const struct mixer_plugin roar_mixer_plugin = {
	.init = roar_mixer_init,
	.finish = roar_mixer_finish,
	.open = roar_mixer_open,
	.close = roar_mixer_close,
	.get_volume = roar_mixer_get_volume,
	.set_volume = roar_mixer_set_volume,
	.global = false,
};
