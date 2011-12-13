/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "null_output_plugin.h"
#include "output_api.h"
#include "timer.h"

#include <glib.h>

#include <assert.h>

struct null_data {
	struct audio_output base;

	bool sync;

	struct timer *timer;
};

static struct audio_output *
null_init(const struct config_param *param, GError **error_r)
{
	struct null_data *nd = g_new(struct null_data, 1);

	if (!ao_base_init(&nd->base, &null_output_plugin, param, error_r)) {
		g_free(nd);
		return NULL;
	}

	nd->sync = config_get_block_bool(param, "sync", true);

	return &nd->base;
}

static void
null_finish(struct audio_output *ao)
{
	struct null_data *nd = (struct null_data *)ao;

	ao_base_finish(&nd->base);
	g_free(nd);
}

static bool
null_open(struct audio_output *ao, struct audio_format *audio_format,
	  G_GNUC_UNUSED GError **error)
{
	struct null_data *nd = (struct null_data *)ao;

	if (nd->sync)
		nd->timer = timer_new(audio_format);

	return true;
}

static void
null_close(struct audio_output *ao)
{
	struct null_data *nd = (struct null_data *)ao;

	if (nd->sync)
		timer_free(nd->timer);
}

static unsigned
null_delay(struct audio_output *ao)
{
	struct null_data *nd = (struct null_data *)ao;

	return nd->sync && nd->timer->started
		? timer_delay(nd->timer)
		: 0;
}

static size_t
null_play(struct audio_output *ao, G_GNUC_UNUSED const void *chunk, size_t size,
	  G_GNUC_UNUSED GError **error)
{
	struct null_data *nd = (struct null_data *)ao;
	struct timer *timer = nd->timer;

	if (!nd->sync)
		return size;

	if (!timer->started)
		timer_start(timer);
	timer_add(timer, size);

	return size;
}

static void
null_cancel(struct audio_output *ao)
{
	struct null_data *nd = (struct null_data *)ao;

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
	.delay = null_delay,
	.play = null_play,
	.cancel = null_cancel,
};
