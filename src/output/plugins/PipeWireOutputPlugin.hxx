// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PIPEWIRE_OUTPUT_PLUGIN_HXX
#define MPD_PIPEWIRE_OUTPUT_PLUGIN_HXX

class PipeWireOutput;
class PipeWireMixer;

extern const struct AudioOutputPlugin pipewire_output_plugin;

void
pipewire_output_set_mixer(PipeWireOutput &po, PipeWireMixer &pm) noexcept;

void
pipewire_output_clear_mixer(PipeWireOutput &po, PipeWireMixer &pm) noexcept;

void
pipewire_output_set_volume(PipeWireOutput &output, float volume);

#endif
