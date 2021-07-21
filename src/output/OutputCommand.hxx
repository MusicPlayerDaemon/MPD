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

/*
 * Glue functions for controlling the audio outputs over the MPD
 * protocol.  These functions perform extra validation on all
 * parameters, because they might be from an untrusted source.
 *
 */

#ifndef MPD_OUTPUT_COMMAND_HXX
#define MPD_OUTPUT_COMMAND_HXX

class MultipleOutputs;

/**
 * Enables an audio output.  Returns false if the specified output
 * does not exist.
 */
bool
audio_output_enable_index(MultipleOutputs &outputs, unsigned idx);

/**
 * Disables an audio output.  Returns false if the specified output
 * does not exist.
 */
bool
audio_output_disable_index(MultipleOutputs &outputs, unsigned idx);

/**
 * Toggles an audio output.  Returns false if the specified output
 * does not exist.
 */
bool
audio_output_toggle_index(MultipleOutputs &outputs, unsigned idx);

#endif
