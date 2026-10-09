// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PW_SINK_MIXER_PLUGIN_HXX
#define MPD_PW_SINK_MIXER_PLUGIN_HXX

struct MixerPlugin;
class PwSinkMixer;

extern const MixerPlugin pw_sink_mixer_plugin;

/**
 * Re-apply the mixer's current volume to the sink. Called from
 * PipeWireOutput::ParamChanged() the first time its stream connects.
 */
void
pw_sink_mixer_request_resync(PwSinkMixer &m) noexcept;

#endif
