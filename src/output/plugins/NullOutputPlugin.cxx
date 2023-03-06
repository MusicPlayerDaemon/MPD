// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

	size_t Play(std::span<const std::byte> src) override {
		if (sync) {
			if (!timer->IsStarted())
				timer->Start();
			timer->Add(src.size());
		}

		return src.size();
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
