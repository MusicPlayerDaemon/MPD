/*
 * Copyright 2020-2021 The Music Player Daemon Project
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

#undef NOUSER // COM needs the "MSG" typedef

#include "output/plugins/wasapi/ForMixer.hxx"
#include "output/plugins/wasapi/AudioClient.hxx"
#include "output/plugins/wasapi/Device.hxx"
#include "mixer/MixerInternal.hxx"
#include "win32/ComPtr.hxx"
#include "win32/ComWorker.hxx"
#include "win32/HResult.hxx"

#include <cmath>
#include <optional>

#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

class WasapiMixer final : public Mixer {
	WasapiOutput &output;

public:
	WasapiMixer(WasapiOutput &_output, MixerListener &_listener)
	: Mixer(wasapi_mixer_plugin, _listener), output(_output) {}

	void Open() override {}

	void Close() noexcept override {}

	int GetVolume() override {
		auto com_worker = wasapi_output_get_com_worker(output);
		if (!com_worker)
			return -1;

		auto future = com_worker->Async([&]() -> int {
			HRESULT result;
			float volume_level;

			if (wasapi_is_exclusive(output)) {
				auto endpoint_volume =
					Activate<IAudioEndpointVolume>(*wasapi_output_get_device(output));

				result = endpoint_volume->GetMasterVolumeLevelScalar(
					&volume_level);
				if (FAILED(result)) {
					throw MakeHResultError(result,
							       "Unable to get master "
							       "volume level");
				}
			} else {
				auto session_volume =
					GetService<ISimpleAudioVolume>(*wasapi_output_get_client(output));

				result = session_volume->GetMasterVolume(&volume_level);
				if (FAILED(result)) {
					throw MakeHResultError(
						result, "Unable to get master volume");
				}
			}

			return std::lround(volume_level * 100.0f);
		});
		return future.get();
	}

	void SetVolume(unsigned volume) override {
		auto com_worker = wasapi_output_get_com_worker(output);
		if (!com_worker)
			throw std::runtime_error("Cannot set WASAPI volume");

		com_worker->Async([&]() {
			HRESULT result;
			const float volume_level = volume / 100.0f;

			if (wasapi_is_exclusive(output)) {
				auto endpoint_volume =
					Activate<IAudioEndpointVolume>(*wasapi_output_get_device(output));

				result = endpoint_volume->SetMasterVolumeLevelScalar(
					volume_level, nullptr);
				if (FAILED(result)) {
					throw MakeHResultError(
						result,
						"Unable to set master volume level");
				}
			} else {
				auto session_volume =
					GetService<ISimpleAudioVolume>(*wasapi_output_get_client(output));

				result = session_volume->SetMasterVolume(volume_level,
									 nullptr);
				if (FAILED(result)) {
					throw MakeHResultError(
						result, "Unable to set master volume");
				}
			}
		}).get();
	}
};

static Mixer *wasapi_mixer_init(EventLoop &, AudioOutput &ao, MixerListener &listener,
				const ConfigBlock &) {
	return new WasapiMixer(wasapi_output_downcast(ao), listener);
}

const MixerPlugin wasapi_mixer_plugin = {
	wasapi_mixer_init,
	false,
};
