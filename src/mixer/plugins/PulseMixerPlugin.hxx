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

#ifndef MPD_PULSE_MIXER_PLUGIN_HXX
#define MPD_PULSE_MIXER_PLUGIN_HXX

class PulseMixer;
struct pa_context;
struct pa_stream;

void
pulse_mixer_on_connect(PulseMixer &pm, pa_context *context);

void
pulse_mixer_on_disconnect(PulseMixer &pm);

void
pulse_mixer_on_change(PulseMixer &pm, pa_context *context, pa_stream *stream);

#endif
