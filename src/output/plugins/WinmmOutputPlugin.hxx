// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_WINMM_OUTPUT_PLUGIN_HXX
#define MPD_WINMM_OUTPUT_PLUGIN_HXX

#include "output/Features.h"

#ifdef ENABLE_WINMM_OUTPUT

#include <windef.h>
#include <mmsystem.h>

class WinmmOutput;

extern const struct AudioOutputPlugin winmm_output_plugin;

[[gnu::pure]]
HWAVEOUT
winmm_output_get_handle(WinmmOutput &output);

#endif

#endif
