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
#include "NullOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "../Wrapper.hxx"
#include "../Timer.hxx"

class NullOutput {
	friend struct AudioOutputWrapper<NullOutput>;

	AudioOutput base;

	const bool sync;

	Timer *timer;

public:
	NullOutput(const ConfigBlock &block)
		:base(null_output_plugin, block),
		 sync(block.GetBlockValue("sync", true)) {}

	static NullOutput *Create(const ConfigBlock &block);

	void Open(AudioFormat &audio_format) {
		if (sync)
			timer = new Timer(audio_format);
	}

	void Close() {
		if (sync)
			delete timer;
	}

	std::chrono::steady_clock::duration Delay() const {
		return sync && timer->IsStarted()
			? timer->GetDelay()
			: std::chrono::steady_clock::duration::zero();
	}

	size_t Play(gcc_unused const void *chunk, size_t size) {
		if (sync) {
			if (!timer->IsStarted())
				timer->Start();
			timer->Add(size);
		}

		return size;
	}

	void Cancel() {
		if (sync)
			timer->Reset();
	}
};

inline NullOutput *
NullOutput::Create(const ConfigBlock &block)
{
	return new NullOutput(block);
}

typedef AudioOutputWrapper<NullOutput> Wrapper;

const struct AudioOutputPlugin null_output_plugin = {
	"null",
	nullptr,
	&Wrapper::Init,
	&Wrapper::Finish,
	nullptr,
	nullptr,
	&Wrapper::Open,
	&Wrapper::Close,
	&Wrapper::Delay,
	nullptr,
	&Wrapper::Play,
	nullptr,
	&Wrapper::Cancel,
	nullptr,
	nullptr,
};
