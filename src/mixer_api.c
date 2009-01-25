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

#include <stdio.h>
#include <assert.h>

#include "mixer_api.h"

struct mixer *
mixer_new(const struct mixer_plugin *plugin, const struct config_param *param)
{
	struct mixer *mixer;

	assert(plugin != NULL);

	mixer = plugin->init(param);

	assert(mixer->plugin == plugin);

	return mixer;
}

void
mixer_free(struct mixer *mixer)
{
	assert(mixer != NULL);
	assert(mixer->plugin != NULL);

	mixer->plugin->finish(mixer);
}

bool mixer_open(struct mixer *mixer)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	return mixer->plugin->open(mixer);
}

bool mixer_control(struct mixer *mixer, int cmd, void *arg)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	return mixer->plugin->control(mixer, cmd, arg);
}

void mixer_close(struct mixer *mixer)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	mixer->plugin->close(mixer);
}
