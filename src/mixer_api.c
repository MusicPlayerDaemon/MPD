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

void mixer_init(struct mixer *mixer, struct mixer_plugin *plugin)
{
	assert(plugin != NULL);
	assert(mixer != NULL);
	mixer->plugin = plugin;
	mixer->data = mixer->plugin->init();
}

void mixer_finish(struct mixer *mixer)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	mixer->plugin->finish(mixer->data);
	mixer->data = NULL;
	mixer->plugin = NULL;
}

void mixer_configure(struct mixer *mixer, const struct config_param *param)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	mixer->plugin->configure(mixer->data, param);
}

bool mixer_open(struct mixer *mixer)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	return mixer->plugin->open(mixer->data);
}

bool mixer_control(struct mixer *mixer, int cmd, void *arg)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	return mixer->plugin->control(mixer->data, cmd, arg);
}

void mixer_close(struct mixer *mixer)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	mixer->plugin->close(mixer->data);
}
