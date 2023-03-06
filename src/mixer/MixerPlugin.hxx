// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * This header declares the mixer_plugin class.  It should not be
 * included directly; use MixerInternal.hxx instead in mixer
 * implementations.
 */

#ifndef MPD_MIXER_PLUGIN_HXX
#define MPD_MIXER_PLUGIN_HXX

struct ConfigBlock;
class AudioOutput;
class Mixer;
class MixerListener;
class EventLoop;

struct MixerPlugin {
	/**
         * Alocates and configures a mixer device.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param ao the associated #AudioOutput
	 * @param param the configuration section
	 * @return a mixer object
	 */
	Mixer *(*init)(EventLoop &event_loop, AudioOutput &ao,
		       MixerListener &listener,
		       const ConfigBlock &block);

	/**
	 * If true, then the mixer is automatically opened, even if
	 * its audio output is not open.  If false, then the mixer is
	 * disabled as long as its audio output is closed.
	 */
	bool global;
};

#endif
