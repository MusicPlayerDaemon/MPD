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
