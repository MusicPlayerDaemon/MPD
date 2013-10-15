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
#include "OutputAPI.hxx"
#include "ConfigError.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <string>

#include <stdio.h>

struct PipeOutput {
	struct audio_output base;

	std::string cmd;
	FILE *fh;

	bool Initialize(const config_param &param, Error &error) {
		return ao_base_init(&base, &pipe_output_plugin, param,
				    error);
	}

	void Deinitialize() {
		ao_base_finish(&base);
	}

	bool Configure(const config_param &param, Error &error);
};

static constexpr Domain pipe_output_domain("pipe_output");

inline bool
PipeOutput::Configure(const config_param &param, Error &error)
{
	cmd = param.GetBlockValue("command", "");
	if (cmd.empty()) {
		error.Set(config_domain,
			  "No \"command\" parameter specified");
		return false;
	}

	return true;
}

static struct audio_output *
pipe_output_init(const config_param &param, Error &error)
{
	PipeOutput *pd = new PipeOutput();

	if (!pd->Initialize(param, error)) {
		delete pd;
		return nullptr;
	}

	if (!pd->Configure(param, error)) {
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

	pd->Deinitialize();
	delete pd;
}

static bool
pipe_output_open(struct audio_output *ao,
		 gcc_unused AudioFormat &audio_format,
		 Error &error)
{
	PipeOutput *pd = (PipeOutput *)ao;

	pd->fh = popen(pd->cmd.c_str(), "w");
	if (pd->fh == nullptr) {
		error.FormatErrno("Error opening pipe \"%s\"",
				  pd->cmd.c_str());
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
pipe_output_play(struct audio_output *ao, const void *chunk, size_t size,
		 Error &error)
{
	PipeOutput *pd = (PipeOutput *)ao;
	size_t ret;

	ret = fwrite(chunk, 1, size, pd->fh);
	if (ret == 0)
		error.SetErrno("Write error on pipe");

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
