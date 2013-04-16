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
#include "PipeOutputPlugin.hxx"
#include "output_api.h"

#include <stdio.h>
#include <errno.h>

struct PipeOutput {
	struct audio_output base;

	char *cmd;
	FILE *fh;

	bool Initialize(const config_param *param, GError **error_r) {
		return ao_base_init(&base, &pipe_output_plugin, param,
				    error_r);
	}

	void Deinitialize() {
		ao_base_finish(&base);
	}

	bool Configure(const config_param *param, GError **error_r);
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
pipe_output_quark(void)
{
	return g_quark_from_static_string("pipe_output");
}

inline bool
PipeOutput::Configure(const config_param *param, GError **error_r)
{
	cmd = config_dup_block_string(param, "command", nullptr);
	if (cmd == nullptr) {
		g_set_error(error_r, pipe_output_quark(), 0,
			    "No \"command\" parameter specified");
		return false;
	}

	return true;
}

static struct audio_output *
pipe_output_init(const config_param *param, GError **error_r)
{
	PipeOutput *pd = new PipeOutput();

	if (!pd->Initialize(param, error_r)) {
		delete pd;
		return nullptr;
	}

	if (!pd->Configure(param, error_r)) {
		pd->Deinitialize();
		delete pd;
		return nullptr;
	}

	return &pd->base;
}

static void
pipe_output_finish(struct audio_output *ao)
{
	PipeOutput *pd = (PipeOutput *)ao;

	g_free(pd->cmd);
	pd->Deinitialize();
	delete pd;
}

static bool
pipe_output_open(struct audio_output *ao,
		 G_GNUC_UNUSED struct audio_format *audio_format,
		 G_GNUC_UNUSED GError **error)
{
	PipeOutput *pd = (PipeOutput *)ao;

	pd->fh = popen(pd->cmd, "w");
	if (pd->fh == nullptr) {
		g_set_error(error, pipe_output_quark(), errno,
			    "Error opening pipe \"%s\": %s",
			    pd->cmd, g_strerror(errno));
		return false;
	}

	return true;
}

static void
pipe_output_close(struct audio_output *ao)
{
	PipeOutput *pd = (PipeOutput *)ao;

	pclose(pd->fh);
}

static size_t
pipe_output_play(struct audio_output *ao, const void *chunk, size_t size, GError **error)
{
	PipeOutput *pd = (PipeOutput *)ao;
	size_t ret;

	ret = fwrite(chunk, 1, size, pd->fh);
	if (ret == 0)
		g_set_error(error, pipe_output_quark(), errno,
			    "Write error on pipe: %s", g_strerror(errno));

	return ret;
}

const struct audio_output_plugin pipe_output_plugin = {
	"pipe",
	nullptr,
	pipe_output_init,
	pipe_output_finish,
	nullptr,
	nullptr,
	pipe_output_open,
	pipe_output_close,
	nullptr,
	nullptr,
	pipe_output_play,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};
