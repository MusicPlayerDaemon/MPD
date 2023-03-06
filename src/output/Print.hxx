// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Protocol specific code for the audio output library.
 *
 */

#ifndef MPD_OUTPUT_PRINT_HXX
#define MPD_OUTPUT_PRINT_HXX

class Response;
class MultipleOutputs;

void
printAudioDevices(Response &r, const MultipleOutputs &outputs);

#endif
