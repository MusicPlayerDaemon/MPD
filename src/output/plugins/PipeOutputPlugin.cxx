/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "../OutputAPI.hxx"
#include "config/ConfigError.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <string>

#include <stdio.h>

class PipeOutput {
	AudioOutput base;

	std::string cmd;
	FILE *fh;

	PipeOutput()
		:base(pipe_output_plugin) {}

	bool Configure(const config_param &param, Error &error);

public:
	static AudioOutput *Create(const config_param &param, Error &error);

	bool Open(AudioFormat &audio_format, Error &error);

	void Close() {
		pclose(fh);
	}

	size_t Play(const void *chunk, size_t size, Error &error);

};

static constexpr Domain pipe_output_domain("pipe_output");

inline bool
PipeOutput::Configure(const config_param &param, Error &error)
{
	if (!base.Configure(param, error))
		return false;

	cmd = param.GetBlockValue("command", "");
	if (cmd.empty()) {
		error.Set(config_domain,
			  "No \"command\" parameter specified");
		return false;
	}

	return true;
}

inline AudioOutput *
PipeOutput::Create(const config_param &param, Error &error)
{
	PipeOutput *po = new PipeOutput();

	if (!po->Configure(param, error)) {
		delete po;
		return nullptr;
	}

	return &po->base;
}

static AudioOutput *
pipe_output_init(const config_param &param, Error &error)
{
	return PipeOutput::Create(param, error);
}

static void
pipe_output_finish(AudioOutput *ao)
{
	PipeOutput *pd = (PipeOutput *)ao;

	delete pd;
}

inline bool
PipeOutput::Open(gcc_unused AudioFormat &audio_format, Error &error)
{
	fh = popen(cmd.c_str(), "w");
	if (fh == nullptr) {
		error.FormatErrno("Error opening pipe \"%s\"",
				  cmd.c_str());
		return false;
	}

	return true;
}

static bool
pipe_output_open(AudioOutput *ao, AudioFormat &audio_format, Error &error)
{
	PipeOutput &po = *(PipeOutput *)ao;

	return po.Open(audio_format, error);
}

static void
pipe_output_close(AudioOutput *ao)
{
	PipeOutput &po = *(PipeOutput *)ao;

	po.Close();
}

inline size_t
PipeOutput::Play(const void *chunk, size_t size, Error &error)
{
	size_t nbytes = fwrite(chunk, 1, size, fh);
	if (nbytes == 0)
		error.SetErrno("Write error on pipe");

	return nbytes;
}

static size_t
pipe_output_play(AudioOutput *ao, const void *chunk, size_t size,
		 Error &error)
{
	PipeOutput &po = *(PipeOutput *)ao;

	return po.Play(chunk, size, error);
}

const struct AudioOutputPlugin pipe_output_plugin = {
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
