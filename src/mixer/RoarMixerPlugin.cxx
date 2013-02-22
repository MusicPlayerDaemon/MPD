/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "MixerInternal.hxx"
#include "output_api.h"
#include "output/RoarOutputPlugin.hxx"

struct RoarMixer {
	/** the base mixer class */
	struct mixer base;
	RoarOutput *self;

	RoarMixer(RoarOutput *_output):self(_output) {
		mixer_init(&base, &roar_mixer_plugin);
	}
};

static struct mixer *
roar_mixer_init(void *ao, gcc_unused const struct config_param *param,
		gcc_unused GError **error_r)
{
	RoarMixer *self = new RoarMixer((RoarOutput *)ao);
	return &self->base;
}

static void
roar_mixer_finish(struct mixer *data)
{
	RoarMixer *self = (RoarMixer *) data;

	delete self;
}

static int
roar_mixer_get_volume(struct mixer *mixer, gcc_unused GError **error_r)
{
	RoarMixer *self = (RoarMixer *)mixer;
	return roar_output_get_volume(self->self);
}

static bool
roar_mixer_set_volume(struct mixer *mixer, unsigned volume,
		      gcc_unused GError **error_r)
{
	RoarMixer *self = (RoarMixer *)mixer;
	return roar_output_set_volume(self->self, volume);
}

const struct mixer_plugin roar_mixer_plugin = {
	roar_mixer_init,
	roar_mixer_finish,
	nullptr,
	nullptr,
	roar_mixer_get_volume,
	roar_mixer_set_volume,
	false,
};
