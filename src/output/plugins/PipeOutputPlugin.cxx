/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "system/Error.hxx"

#include <string>
#include <stdexcept>

#include <stdio.h>

class PipeOutput {
	friend struct AudioOutputWrapper<PipeOutput>;

	AudioOutput base;

	const std::string cmd;
	FILE *fh;

	PipeOutput(const ConfigBlock &block);

public:
	static PipeOutput *Create(const ConfigBlock &block);

	void Open(AudioFormat &audio_format);

	void Close() {
		pclose(fh);
	}

	size_t Play(const void *chunk, size_t size);
};

PipeOutput::PipeOutput(const ConfigBlock &block)
	:base(pipe_output_plugin, block),
	 cmd(block.GetBlockValue("command", ""))
{
	if (cmd.empty())
		throw std::runtime_error("No \"command\" parameter specified");
}

inline PipeOutput *
PipeOutput::Create(const ConfigBlock &block)
{
	return new PipeOutput(block);
}

inline void
PipeOutput::Open(gcc_unused AudioFormat &audio_format)
{
	fh = popen(cmd.c_str(), "w");
	if (fh == nullptr)
		throw FormatErrno("Error opening pipe \"%s\"", cmd.c_str());
}

inline size_t
PipeOutput::Play(const void *chunk, size_t size)
{
	size_t nbytes = fwrite(chunk, 1, size, fh);
	if (nbytes == 0)
		throw MakeErrno("Write error on pipe");

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
