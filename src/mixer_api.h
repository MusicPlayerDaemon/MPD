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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MPD_MIXER_H
#define MPD_MIXER_H

#include <stdbool.h>

/*
 * list of currently implemented mixers
 */

extern const struct mixer_plugin alsa_mixer;
extern const struct mixer_plugin oss_mixer;

struct config_param;

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

struct mixer {
	const struct mixer_plugin *plugin;
};

static inline void
mixer_init(struct mixer *mixer, const struct mixer_plugin *plugin)
{
	mixer->plugin = plugin;
}

struct mixer *
mixer_new(const struct mixer_plugin *plugin, const struct config_param *param);

void
mixer_free(struct mixer *mixer);

bool mixer_open(struct mixer *mixer);
void mixer_close(struct mixer *mixer);

static inline int
mixer_get_volume(struct mixer *mixer)
{
	return mixer->plugin->get_volume(mixer);
}

static inline bool
mixer_set_volume(struct mixer *mixer, unsigned volume)
{
	return mixer->plugin->set_volume(mixer, volume);
}

#endif
