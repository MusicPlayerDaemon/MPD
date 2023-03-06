// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PULSE_OUTPUT_PLUGIN_HXX
#define MPD_PULSE_OUTPUT_PLUGIN_HXX

class PulseOutput;
class PulseMixer;
struct pa_cvolume;

extern const struct AudioOutputPlugin pulse_output_plugin;

struct pa_threaded_mainloop *
pulse_output_get_mainloop(PulseOutput &po);

void
pulse_output_set_mixer(PulseOutput &po, PulseMixer &pm);

void
pulse_output_clear_mixer(PulseOutput &po, PulseMixer &pm);

void
pulse_output_set_volume(PulseOutput &po, const pa_cvolume *volume);

#endif
