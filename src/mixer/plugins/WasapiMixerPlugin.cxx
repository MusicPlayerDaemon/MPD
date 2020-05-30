/*
 * Copyright 2020 The Music Player Daemon Project
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

#include "mixer/MixerInternal.hxx"
#include "output/plugins/WasapiOutputPlugin.hxx"
#include "win32/Com.hxx"
#include "win32/HResult.hxx"

#include <cmath>
#include <endpointvolume.h>
#include <optional>

class WasapiMixer final : public Mixer {
	WasapiOutput &output;
	std::optional<COM> com;

public:
	WasapiMixer(WasapiOutput &_output, MixerListener &_listener)
	: Mixer(wasapi_mixer_plugin, _listener), output(_output) {}

	void Open() override { com.emplace(); }

	void Close() noexcept override { com.reset(); }

	int GetVolume() override {
		HRESULT result;
		float volume_level;

		if (wasapi_is_exclusive(output)) {
			ComPtr<IAudioEndpointVolume> endpoint_volume;
			result = wasapi_output_get_device(output)->Activate(
				__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
				endpoint_volume.AddressCast());
			if (FAILED(result)) {
				throw FormatHResultError(
					result, "Unable to get device endpoint volume");
			}

			result = endpoint_volume->GetMasterVolumeLevelScalar(
				&volume_level);
			if (FAILED(result)) {
				throw FormatHResultError(
					result, "Unable to get master volume level");
			}
		} else {
			ComPtr<ISimpleAudioVolume> session_volume;
			result = wasapi_output_get_client(output)->GetService(
				__uuidof(ISimpleAudioVolume),
				session_volume.AddressCast<void>());
			if (FAILED(result)) {
				throw FormatHResultError(
					result, "Unable to get client session volume");
			}

			result = session_volume->GetMasterVolume(&volume_level);
			if (FAILED(result)) {
				throw FormatHResultError(result,
							 "Unable to get master volume");
			}
		}

		return std::lround(volume_level * 100.0f);
	}

	void SetVolume(unsigned volume) override {
		HRESULT result;
		const float volume_level = volume / 100.0f;

		if (wasapi_is_exclusive(output)) {
			ComPtr<IAudioEndpointVolume> endpoint_volume;
			result = wasapi_output_get_device(output)->Activate(
				__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
				endpoint_volume.AddressCast());
			if (FAILED(result)) {
				throw FormatHResultError(
					result, "Unable to get device endpoint volume");
			}

			result = endpoint_volume->SetMasterVolumeLevelScalar(volume_level,
									     nullptr);
			if (FAILED(result)) {
				throw FormatHResultError(
					result, "Unable to set master volume level");
			}
		} else {
			ComPtr<ISimpleAudioVolume> session_volume;
			result = wasapi_output_get_client(output)->GetService(
				__uuidof(ISimpleAudioVolume),
				session_volume.AddressCast<void>());
			if (FAILED(result)) {
				throw FormatHResultError(
					result, "Unable to get client session volume");
			}

			result = session_volume->SetMasterVolume(volume_level, nullptr);
			if (FAILED(result)) {
				throw FormatHResultError(result,
							 "Unable to set master volume");
			}
		}
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
