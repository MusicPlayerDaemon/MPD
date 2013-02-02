/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "NullOutputPlugin.hxx"
#include "output_api.h"
#include "timer.h"

#include <glib.h>

#include <assert.h>

struct NullOutput {
	struct audio_output base;

	bool sync;

	struct timer *timer;
};

static struct audio_output *
null_init(const struct config_param *param, GError **error_r)
{
	NullOutput *nd = g_new(NullOutput, 1);

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
	NullOutput *nd = (NullOutput *)ao;

	ao_base_finish(&nd->base);
	g_free(nd);
}

static bool
null_open(struct audio_output *ao, struct audio_format *audio_format,
	  G_GNUC_UNUSED GError **error)
{
	NullOutput *nd = (NullOutput *)ao;

	if (nd->sync)
		nd->timer = timer_new(audio_format);

	return true;
}

static void
null_close(struct audio_output *ao)
{
	NullOutput *nd = (NullOutput *)ao;

	if (nd->sync)
		timer_free(nd->timer);
}

static unsigned
null_delay(struct audio_output *ao)
{
	NullOutput *nd = (NullOutput *)ao;

	return nd->sync && nd->timer->started
		? timer_delay(nd->timer)
		: 0;
}

static size_t
null_play(struct audio_output *ao, G_GNUC_UNUSED const void *chunk, size_t size,
	  G_GNUC_UNUSED GError **error)
{
	NullOutput *nd = (NullOutput *)ao;
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
	NullOutput *nd = (NullOutput *)ao;

	if (!nd->sync)
		return;

	timer_reset(nd->timer);
}

const struct audio_output_plugin null_output_plugin = {
	"null",
	nullptr,
	null_init,
	null_finish,
	nullptr,
	nullptr,
	null_open,
	null_close,
	null_delay,
	nullptr,
	null_play,
	nullptr,
	null_cancel,
	nullptr,
	nullptr,
};
