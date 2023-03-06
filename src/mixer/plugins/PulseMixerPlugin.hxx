// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

struct MixerPlugin;
class PulseMixer;
struct pa_context;
struct pa_stream;

extern const MixerPlugin pulse_mixer_plugin;

void
pulse_mixer_on_connect(PulseMixer &pm, pa_context *context);

void
pulse_mixer_on_disconnect(PulseMixer &pm);

void
pulse_mixer_on_change(PulseMixer &pm, pa_context *context, pa_stream *stream);
