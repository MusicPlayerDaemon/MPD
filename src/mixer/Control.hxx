// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * Functions which manipulate a #Mixer object.
 */

#ifndef MPD_MIXER_CONTROL_HXX
#define MPD_MIXER_CONTROL_HXX

class Mixer;
class EventLoop;
class AudioOutput;
struct MixerPlugin;
class MixerListener;
struct ConfigBlock;

/**
 * Throws std::runtime_error on error.
 */
Mixer *
mixer_new(EventLoop &event_loop, const MixerPlugin &plugin,
	  AudioOutput &ao,
	  MixerListener &listener,
	  const ConfigBlock &block);

void
mixer_free(Mixer *mixer) noexcept;

#endif
