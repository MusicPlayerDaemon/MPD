/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "NullOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "../Timer.hxx"

class NullOutput final  : AudioOutput {
	const bool sync;

	Timer *timer;

public:
	explicit NullOutput(const ConfigBlock &block)
		:AudioOutput(0),
		 sync(block.GetBlockValue("sync", true)) {}

	static AudioOutput *Create(EventLoop &,
				   const ConfigBlock &block) {
		return new NullOutput(block);
	}

private:
	void Open(AudioFormat &audio_format) override {
		if (sync)
			timer = new Timer(audio_format);
	}

	void Close() noexcept override {
		if (sync)
			delete timer;
	}

	[[nodiscard]] std::chrono::steady_clock::duration Delay() const noexcept override {
		return sync && timer->IsStarted()
			? timer->GetDelay()
			: std::chrono::steady_clock::duration::zero();
	}

	size_t Play([[maybe_unused]] const void *chunk, size_t size) override {
		if (sync) {
			if (!timer->IsStarted())
				timer->Start();
			timer->Add(size);
		}

		return size;
	}

	void Cancel() noexcept override {
		if (sync)
			timer->Reset();
	}
};

const struct AudioOutputPlugin null_output_plugin = {
	"null",
	nullptr,
	&NullOutput::Create,
	nullptr,
};
