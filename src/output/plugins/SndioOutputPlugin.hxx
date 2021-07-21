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

#ifndef MPD_SNDIO_OUTPUT_PLUGIN_HXX
#define MPD_SNDIO_OUTPUT_PLUGIN_HXX

#include "../OutputAPI.hxx"

class Mixer;
class MixerListener;

extern const struct AudioOutputPlugin sndio_output_plugin;

class SndioOutput final : AudioOutput {
	Mixer *mixer = nullptr;
	MixerListener *listener = nullptr;
	const char *const device;
	const unsigned buffer_time; /* in ms */
	struct sio_hdl *hdl;
	int raw_volume;

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
	size_t Play(const void *chunk, size_t size) override;
};

#endif
