// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_WASAPI_OUTPUT_FOR_MIXER_HXX
#define MPD_WASAPI_OUTPUT_FOR_MIXER_HXX

#include <memory>

struct IMMDevice;
struct IAudioClient;
class AudioOutput;
class WasapiOutput;
class COMWorker;

[[gnu::pure]]
WasapiOutput &
wasapi_output_downcast(AudioOutput &output) noexcept;

[[gnu::pure]]
bool
wasapi_is_exclusive(WasapiOutput &output) noexcept;

[[gnu::pure]]
std::shared_ptr<COMWorker>
wasapi_output_get_com_worker(WasapiOutput &output) noexcept;

[[gnu::pure]]
IMMDevice *
wasapi_output_get_device(WasapiOutput &output) noexcept;

[[gnu::pure]]
IAudioClient *
wasapi_output_get_client(WasapiOutput &output) noexcept;

#endif
