// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_AUDIO_OUTPUT_ERROR_HXX
#define MPD_AUDIO_OUTPUT_ERROR_HXX

/**
 * An exception class that will be thrown by various #AudioOutput
 * methods after AudioOutput::Interrupt() has been called.
 */
class AudioOutputInterrupted {};

#endif
