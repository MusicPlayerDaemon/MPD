/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
