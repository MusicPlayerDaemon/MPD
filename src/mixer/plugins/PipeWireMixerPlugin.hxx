// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PIPEWIRE_MIXER_PLUGIN_HXX
#define MPD_PIPEWIRE_MIXER_PLUGIN_HXX

struct MixerPlugin;
class PipeWireMixer;

extern const MixerPlugin pipewire_mixer_plugin;

void
pipewire_mixer_on_change(PipeWireMixer &pm, float new_volume) noexcept;

#endif
