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

#ifndef MPD_WASAPI_OUTPUT_PLUGIN_HXX
#define MPD_WASAPI_OUTPUT_PLUGIN_HXX

#include "output/Features.h"

#include "../OutputAPI.hxx"
#include "util/Compiler.h"
#include "win32/ComPtr.hxx"

#include <audioclient.h>
#include <mmdeviceapi.h>

extern const struct AudioOutputPlugin wasapi_output_plugin;

class WasapiOutput;

gcc_pure WasapiOutput &wasapi_output_downcast(AudioOutput &output) noexcept;

gcc_pure bool wasapi_is_exclusive(WasapiOutput &output) noexcept;

gcc_pure IMMDevice *wasapi_output_get_device(WasapiOutput &output) noexcept;

gcc_pure IAudioClient *wasapi_output_get_client(WasapiOutput &output) noexcept;

#endif
