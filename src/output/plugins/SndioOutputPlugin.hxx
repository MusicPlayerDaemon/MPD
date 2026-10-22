// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SNDIO_OUTPUT_PLUGIN_HXX
#define MPD_SNDIO_OUTPUT_PLUGIN_HXX

#include "../OutputAPI.hxx"

#include <atomic>

class Mixer;
class MixerListener;

extern const struct AudioOutputPlugin sndio_output_plugin;

class SndioOutput final : AudioOutput {
	Mixer *mixer = nullptr;
	MixerListener *listener = nullptr;
	const char *const device;
	const unsigned buffer_time; /* in ms */
	struct sio_hdl *hdl;

	/**
	 * Written by VolumeChanged() in the output thread, read by
	 * GetVolume() in the main thread.
	 */
	std::atomic_int raw_volume;

	/**
	 * A volume set by SetVolume() which has not yet been applied
	 * by Play(), or -1.  libsndio handles are not thread-safe, so
	 * only the output thread may call sio_setvol().
	 */
	std::atomic_int pending_raw_volume = -1;

public:
	SndioOutput(const ConfigBlock &block);

	static AudioOutput *Create(EventLoop &,
		const ConfigBlock &block);

	void SetVolume(unsigned int _volume);
	unsigned int GetVolume();
	void VolumeChanged(int _volume);
	void RegisterMixerListener(Mixer *_mixer, MixerListener *_listener);

private:
	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;
	size_t Play(std::span<const std::byte> src) override;
};

#endif
