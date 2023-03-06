// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "WinmmMixerPlugin.hxx"
#include "mixer/Mixer.hxx"
#include "output/Features.h"
#include "output/OutputAPI.hxx"
#include "output/plugins/WinmmOutputPlugin.hxx"
#include "util/Math.hxx"

#include <mmsystem.h>

#include <cassert>
#include <stdexcept>

#include <windows.h>

class WinmmMixer final : public Mixer {
	WinmmOutput &output;

public:
	WinmmMixer(WinmmOutput &_output, MixerListener &_listener)
		:Mixer(winmm_mixer_plugin, _listener),
		output(_output) {
	}

	/* virtual methods from class Mixer */
	void Open() override {
	}

	void Close() noexcept override {
	}

	int GetVolume() override;
	void SetVolume(unsigned volume) override;
};

static inline int
winmm_volume_decode(DWORD volume)
{
	return lround((volume & 0xFFFF) / 655.35);
}

static inline DWORD
winmm_volume_encode(int volume)
{
	int value = lround(volume * 655.35);
	return MAKELONG(value, value);
}

static Mixer *
winmm_mixer_init([[maybe_unused]] EventLoop &event_loop, AudioOutput &ao,
		 MixerListener &listener,
		 [[maybe_unused]] const ConfigBlock &block)
{
	return new WinmmMixer((WinmmOutput &)ao, listener);
}

int
WinmmMixer::GetVolume()
{
	DWORD volume;
	HWAVEOUT handle = winmm_output_get_handle(output);
	MMRESULT result = waveOutGetVolume(handle, &volume);

	if (result != MMSYSERR_NOERROR)
		throw std::runtime_error("Failed to get winmm volume");

	return winmm_volume_decode(volume);
}

void
WinmmMixer::SetVolume(unsigned volume)
{
	DWORD value = winmm_volume_encode(volume);
	HWAVEOUT handle = winmm_output_get_handle(output);
	MMRESULT result = waveOutSetVolume(handle, value);

	if (result != MMSYSERR_NOERROR)
		throw std::runtime_error("Failed to set winmm volume");
}

const MixerPlugin winmm_mixer_plugin = {
	winmm_mixer_init,
	false,
};
