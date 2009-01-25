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

#include "conf.h"

/*
 * list of currently implemented mixers
 */

extern const struct mixer_plugin alsa_mixer;
extern const struct mixer_plugin oss_mixer;

struct mixer_data;

struct mixer_plugin {

        /**
         * Allocate and initialize mixer data
	 */
        struct mixer_data *(*init)(void);

        /**
	 * Finish and free mixer data
         */
        void (*finish)(struct mixer_data *data);

        /**
	 * Setup and configure mixer
         */
	void (*configure)(struct mixer_data *data,
			  const struct config_param *param);

        /**
    	 * Open mixer device
	 */
	bool (*open)(struct mixer_data *data);

        /**
	 * Control mixer device.
         */
	bool (*control)(struct mixer_data *data, int cmd, void *arg);

        /**
    	 * Close mixer device
	 */
	void (*close)(struct mixer_data *data);
};

struct mixer {
	const struct mixer_plugin *plugin;
	struct mixer_data *data;
};

void mixer_init(struct mixer *mixer, const struct mixer_plugin *plugin);
void mixer_finish(struct mixer *mixer);

struct mixer *
mixer_new(const struct mixer_plugin *plugin);

void
mixer_free(struct mixer *mixer);

void mixer_configure(struct mixer *mixer, const struct config_param *param);
bool mixer_open(struct mixer *mixer);
bool mixer_control(struct mixer *mixer, int cmd, void *arg);
void mixer_close(struct mixer *mixer);

#endif
