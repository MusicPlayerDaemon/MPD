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

#include <stdio.h>
#include <errno.h>

struct pipe_output {
	char *cmd;
	FILE *fh;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
pipe_output_quark(void)
{
	return g_quark_from_static_string("pipe_output");
}

static void *
pipe_output_init(G_GNUC_UNUSED const struct audio_format *audio_format,
		 const struct config_param *param,
		 GError **error)
{
	struct pipe_output *pd = g_new(struct pipe_output, 1);

	pd->cmd = config_dup_block_string(param, "command", NULL);
	if (pd->cmd == NULL) {
		g_set_error(error, pipe_output_quark(), 0,
			    "No \"command\" parameter specified");
		return NULL;
	}

	return pd;
}

static void
pipe_output_finish(void *data)
{
	struct pipe_output *pd = data;

	g_free(pd->cmd);
	g_free(pd);
}

static bool
pipe_output_open(void *data, G_GNUC_UNUSED struct audio_format *audio_format,
		 G_GNUC_UNUSED GError **error)
{
	struct pipe_output *pd = data;

	pd->fh = popen(pd->cmd, "w");
	if (pd->fh == NULL) {
		g_set_error(error, pipe_output_quark(), errno,
			    "Error opening pipe \"%s\": %s",
			    pd->cmd, g_strerror(errno));
		return false;
	}

	return true;
}

static void
pipe_output_close(void *data)
{
	struct pipe_output *pd = data;

	pclose(pd->fh);
}

static size_t
pipe_output_play(void *data, const void *chunk, size_t size, GError **error)
{
	struct pipe_output *pd = data;
	size_t ret;

	ret = fwrite(chunk, 1, size, pd->fh);
	if (ret == 0)
		g_set_error(error, pipe_output_quark(), errno,
			    "Write error on pipe: %s", g_strerror(errno));

	return ret;
}

const struct audio_output_plugin pipe_output_plugin = {
	.name = "pipe",
	.init = pipe_output_init,
	.finish = pipe_output_finish,
	.open = pipe_output_open,
	.close = pipe_output_close,
	.play = pipe_output_play,
};
