// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Saving and loading the audio output states to/from the state file.
 *
 */

#ifndef MPD_OUTPUT_STATE_HXX
#define MPD_OUTPUT_STATE_HXX

class MultipleOutputs;
class BufferedOutputStream;

bool
audio_output_state_read(const char *line, MultipleOutputs &outputs);

void
audio_output_state_save(BufferedOutputStream &os,
			const MultipleOutputs &outputs);

/**
 * Generates a version number for the current state of the audio
 * outputs.  This is used by timer_save_state_file() to determine
 * whether the state has changed and the state file should be saved.
 */
unsigned
audio_output_state_get_version();

#endif
