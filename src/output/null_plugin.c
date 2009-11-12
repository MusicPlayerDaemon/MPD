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
#include "output_api.h"
#include "timer.h"

#include <glib.h>

#include <assert.h>

struct null_data {
	bool sync;

	Timer *timer;
};

static void *
null_init(G_GNUC_UNUSED const struct audio_format *audio_format,
	  G_GNUC_UNUSED const struct config_param *param,
	  G_GNUC_UNUSED GError **error)
{
	struct null_data *nd = g_new(struct null_data, 1);

	nd->sync = config_get_block_bool(param, "sync", true);
	nd->timer = NULL;

	return nd;
}

static void
null_finish(void *data)
{
	struct null_data *nd = data;

	assert(nd->timer == NULL);

	g_free(nd);
}

static bool
null_open(void *data, struct audio_format *audio_format,
	  G_GNUC_UNUSED GError **error)
{
	struct null_data *nd = data;

	if (nd->sync)
		nd->timer = timer_new(audio_format);

	return true;
}

static void
null_close(void *data)
{
	struct null_data *nd = data;

	if (nd->timer != NULL) {
		timer_free(nd->timer);
		nd->timer = NULL;
	}
}

static size_t
null_play(void *data, G_GNUC_UNUSED const void *chunk, size_t size,
	  G_GNUC_UNUSED GError **error)
{
	struct null_data *nd = data;
	Timer *timer = nd->timer;

	if (!nd->sync)
		return size;

	if (!timer->started)
		timer_start(timer);
	else
		timer_sync(timer);

	timer_add(timer, size);

	return size;
}

static void
null_cancel(void *data)
{
	struct null_data *nd = data;

	if (!nd->sync)
		return;

	timer_reset(nd->timer);
}

const struct audio_output_plugin null_output_plugin = {
	.name = "null",
	.init = null_init,
	.finish = null_finish,
	.open = null_open,
	.close = null_close,
	.play = null_play,
	.cancel = null_cancel,
};
