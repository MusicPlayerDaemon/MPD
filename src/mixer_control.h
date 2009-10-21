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

/** \file
 *
 * Functions which manipulate a #mixer object.
 */

#ifndef MPD_MIXER_CONTROL_H
#define MPD_MIXER_CONTROL_H

#include <glib.h>

#include <stdbool.h>

struct mixer;
struct mixer_plugin;
struct config_param;

struct mixer *
mixer_new(const struct mixer_plugin *plugin, void *ao,
	  const struct config_param *param,
	  GError **error_r);

void
mixer_free(struct mixer *mixer);

bool
mixer_open(struct mixer *mixer, GError **error_r);

void
mixer_close(struct mixer *mixer);

/**
 * Close the mixer unless the plugin's "global" flag is set.  This is
 * called when the #audio_output is closed.
 */
void
mixer_auto_close(struct mixer *mixer);

int
mixer_get_volume(struct mixer *mixer, GError **error_r);

bool
mixer_set_volume(struct mixer *mixer, unsigned volume, GError **error_r);

#endif
