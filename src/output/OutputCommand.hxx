// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Glue functions for controlling the audio outputs over the MPD
 * protocol.  These functions perform extra validation on all
 * parameters, because they might be from an untrusted source.
 *
 */

#ifndef MPD_OUTPUT_COMMAND_HXX
#define MPD_OUTPUT_COMMAND_HXX

class MultipleOutputs;
class MixerMemento;

/**
 * Enables an audio output.  Returns false if the specified output
 * does not exist.
 */
bool
audio_output_enable_index(MultipleOutputs &outputs,
			  MixerMemento &mixer_memento,
			  unsigned idx);

/**
 * Disables an audio output.  Returns false if the specified output
 * does not exist.
 */
bool
audio_output_disable_index(MultipleOutputs &outputs,
			   MixerMemento &mixer_memento,
			   unsigned idx);

/**
 * Toggles an audio output.  Returns false if the specified output
 * does not exist.
 */
bool
audio_output_toggle_index(MultipleOutputs &outputs,
			  MixerMemento &mixer_memento,
			  unsigned idx);

#endif
