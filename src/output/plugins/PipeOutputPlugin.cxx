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
#include <sstream>

#include <stdio.h>

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

/* helper function */
static void replaceString(std::string& str, const std::string& from,
                          const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

inline bool
PipeOutput::Open(gcc_unused AudioFormat &audio_format, Error &error)
{
        std::string replacedString = cmd;
        std::string bits, format;
        std::string channels;
        std::string sampleRate;

        switch(audio_format.format)
        {
                case SampleFormat::S8: format = "S8"; bits = "8"; break;
                case SampleFormat::S16: format = "S16"; bits = "16"; break;
                case SampleFormat::S24_P32: format ="S24_P32"; bits = 32; break;
                case SampleFormat::S32: format = "S32"; bits = "32"; break;
                case SampleFormat::FLOAT: format = "FLOAT"; bits = "32"; break;
                case SampleFormat::DSD: format = "DSD"; bits = "32"; break;
                case SampleFormat::UNDEFINED: format = "UNDEFINED"; bits = "16"; break;
        }
        channels = std::to_string( audio_format.channels );
        sampleRate = std::to_string( audio_format.sample_rate );

        replaceString(replacedString,"$MPDPIPE_BITS", bits);
        replaceString(replacedString,"$MPDPIPE_CHANNELS", channels);
        replaceString(replacedString,"$MPDPIPE_RATE", sampleRate);
        replaceString(replacedString,"$MPDPIPE_FORMAT", format);

        fh = popen(replacedString.c_str(), "w");
        if (fh == nullptr) {
                error.FormatErrno("Error opening pipe \"%s\"",
                                  replacedString.c_str());
                return false;
        }

        return true;
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
