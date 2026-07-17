// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

/*
 * Glue functions for controlling the audio outputs over the MPD
 * protocol.  These functions perform extra validation on all
 * parameters, because they might be from an untrusted source.
 *
 */

struct Partition;
class AudioOutputControl;

/**
 * Look up an output of the specified partition and throw
 * #ProtocolError on error.
 */
AudioOutputControl &
CheckPartitionOutput(Partition &partition, unsigned idx);

/**
 * Enables an audio output.
 *
 * Throws #ProtocolError if the specified output
 * does not exist.
 */
void
audio_output_enable_index(Partition &partition,
			  unsigned idx);

/**
 * Disables an audio output.
 *
 * Throws #ProtocolError if the specified output
 * does not exist.
 */
void
audio_output_disable_index(Partition &partition,
			   unsigned idx);

/**
 * Toggles an audio output.
 *
 * Throws #ProtocolError if the specified output
 * does not exist.
 */
void
audio_output_toggle_index(Partition &partition,
			  unsigned idx);
