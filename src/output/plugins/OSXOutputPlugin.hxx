// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OSX_OUTPUT_PLUGIN_HXX
#define MPD_OSX_OUTPUT_PLUGIN_HXX

struct OSXOutput;

extern const struct AudioOutputPlugin osx_output_plugin;

int
osx_output_get_volume(OSXOutput &output);

void
osx_output_set_volume(OSXOutput &output, unsigned new_volume);

#endif
