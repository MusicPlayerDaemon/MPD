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

#include "../output/raop_output_plugin.h"
#include "output_plugin.h"
#include "mixer_api.h"

struct raop_mixer_plugin {
	struct mixer base;
	struct raop_data *rd;
};

static struct mixer *
raop_mixer_init(void *ao, G_GNUC_UNUSED const struct config_param *param,
		 G_GNUC_UNUSED GError **error_r)
{
	struct raop_mixer_plugin *rm = g_new(struct raop_mixer_plugin, 1);
	rm->rd = (struct raop_data *) ao;
	mixer_init(&rm->base, &raop_mixer_plugin);

	return &rm->base;
}

static void
raop_mixer_finish(struct mixer *data)
{
	struct raop_mixer_plugin *rm = (struct raop_mixer_plugin *) data;

	g_free(rm);
}

static int
raop_mixer_get_volume(struct mixer *mixer, G_GNUC_UNUSED GError **error_r)
{
	struct raop_mixer_plugin *rm = (struct raop_mixer_plugin *)mixer;
	return raop_get_volume(rm->rd);
}

static bool
raop_mixer_set_volume(struct mixer *mixer, unsigned volume, GError **error_r)
{
	struct raop_mixer_plugin *rm = (struct raop_mixer_plugin *)mixer;
	return raop_set_volume(rm->rd, volume, error_r);
}

const struct mixer_plugin raop_mixer_plugin = {
	.init = raop_mixer_init,
	.finish = raop_mixer_finish,
	.get_volume = raop_mixer_get_volume,
	.set_volume = raop_mixer_set_volume,
};
