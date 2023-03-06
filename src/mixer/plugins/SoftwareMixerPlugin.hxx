// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

struct MixerPlugin;
class Mixer;
class Filter;

extern const MixerPlugin software_mixer_plugin;

/**
 * Attach a #VolumeFilter to this mixer.  The #VolumeFilter is the
 * entity which actually applies the volume; it is created and managed
 * by the output.  Mixer::SetVolume() calls will be forwarded to
 * volume_filter_set().
 */
void
software_mixer_set_filter(Mixer &mixer, Filter *filter) noexcept;
