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
 * This header declares the mixer_plugin class.  It should not be
 * included directly; use mixer_api.h instead in mixer
 * implementations.
 */

#ifndef MPD_MIXER_PLUGIN_H
#define MPD_MIXER_PLUGIN_H

#include <stdbool.h>

struct config_param;
struct mixer;

struct mixer_plugin {
	/**
         * Alocates and configures a mixer device.
	 */
	struct mixer *(*init)(const struct config_param *param);

	/**
	 * Finish and free mixer data
         */
	void (*finish)(struct mixer *data);

	/**
	 * Open mixer device
	 */
	bool (*open)(struct mixer *data);

	/**
	 * Close mixer device
	 */
	void (*close)(struct mixer *data);

	/**
	 * Reads the current volume.
	 *
	 * @return the current volume (0..100 including) or -1 on
	 * error
	 */
	int (*get_volume)(struct mixer *mixer);

	/**
	 * Sets the volume.
	 *
	 * @param volume the new volume (0..100 including)
	 * @return true on success
	 */
	bool (*set_volume)(struct mixer *mixer, unsigned volume);
};

#endif
