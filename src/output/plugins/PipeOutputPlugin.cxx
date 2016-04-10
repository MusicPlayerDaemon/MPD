/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "../Wrapper.hxx"
#include "config/ConfigError.hxx"
#include "util/Error.hxx"

#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

class PipeOutput {
	friend struct AudioOutputWrapper<PipeOutput>;

	AudioOutput base;

	std::string cmd;
	FILE *fh;

	PipeOutput()
		:base(pipe_output_plugin) {}

	bool Configure(const ConfigBlock &block, Error &error);

public:
	static PipeOutput *Create(const ConfigBlock &block, Error &error);

	bool Open(AudioFormat &audio_format, Error &error);

	void Close() {
		pclose(fh);
	}

	size_t Play(const void *chunk, size_t size, Error &error);
};

inline bool
PipeOutput::Configure(const ConfigBlock &block, Error &error)
{
	if (!base.Configure(block, error))
		return false;

	cmd = block.GetBlockValue("command", "");
	if (cmd.empty()) {
		error.Set(config_domain,
			  "No \"command\" parameter specified");
		return false;
	}

	return true;
}

inline PipeOutput *
PipeOutput::Create(const ConfigBlock &block, Error &error)
{
	PipeOutput *po = new PipeOutput();

	if (!po->Configure(block, error)) {
		delete po;
		return nullptr;
	}

	return po;
}

inline bool
PipeOutput::Open(AudioFormat &audio_format, Error &error)
{
	int pfdes[2];
	pid_t childpid;
	char strbuf[8];

	if (pipe(pfdes) == -1) {
		error.FormatErrno("Error opening pipe for output.");
		return false;
	}
	childpid = fork();
	if (childpid == -1) {
		error.FormatErrno("Error forking for pipe output.");
		return false;
	}
	if (childpid != 0) {
		close(pfdes[0]);
		fh = fdopen(pfdes[1], "w");
		if (fh == NULL) {
			error.FormatErrno("Error creating file pointer for pipe output.");
			return false;
		}
		return true;
	} else {
		close(pfdes[1]);
		dup2(pfdes[0], 0);
		close(pfdes[0]);
		setenv("MPDPIPE_BITS", sample_format_to_string(audio_format.format), 1);
		snprintf(strbuf, sizeof(strbuf), "%u", audio_format.sample_rate);
		setenv("MPDPIPE_RATE", strbuf, 1);
		snprintf(strbuf, sizeof(strbuf), "%u", audio_format.channels);
		setenv("MPDPIPE_CHANNELS", strbuf, 1);
		execlp("sh", "/bin/sh", "-c", cmd.c_str(), (char*)NULL);
		error.FormatErrno("Cannot execute pipe program \"%s\"", cmd.c_str());
		abort();
	}
}

inline size_t
PipeOutput::Play(const void *chunk, size_t size, Error &error)
{
	size_t nbytes = fwrite(chunk, 1, size, fh);
	if (nbytes == 0)
		error.SetErrno("Write error on pipe");

	return nbytes;
}

typedef AudioOutputWrapper<PipeOutput> Wrapper;

const struct AudioOutputPlugin pipe_output_plugin = {
	"pipe",
	nullptr,
	&Wrapper::Init,
	&Wrapper::Finish,
	nullptr,
	nullptr,
	&Wrapper::Open,
	&Wrapper::Close,
	nullptr,
	nullptr,
	&Wrapper::Play,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};
