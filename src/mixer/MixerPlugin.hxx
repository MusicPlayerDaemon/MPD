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
