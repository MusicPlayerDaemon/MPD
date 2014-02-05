/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_MIXER_INTERNAL_HXX
#define MPD_MIXER_INTERNAL_HXX

#include "MixerPlugin.hxx"
#include "MixerList.hxx"
#include "thread/Mutex.hxx"
#include "Compiler.h"

class MixerListener;

class Mixer {
public:
	const MixerPlugin &plugin;

	MixerListener &listener;

	/**
	 * This mutex protects all of the mixer struct, including its
	 * implementation, so plugins don't have to deal with that.
	 */
	Mutex mutex;

	/**
	 * Is the mixer device currently open?
	 */
	bool open;

	/**
	 * Has this mixer failed, and should not be reopened
	 * automatically?
	 */
	bool failed;

public:
	explicit Mixer(const MixerPlugin &_plugin, MixerListener &_listener)
		:plugin(_plugin), listener(_listener),
		 open(false),
		 failed(false) {}

	Mixer(const Mixer &) = delete;

	virtual ~Mixer() {}

	bool IsPlugin(const MixerPlugin &other) const {
		return &plugin == &other;
	}

	/**
	 * Open mixer device
	 *
	 * @return true on success, false on error
	 */
	virtual bool Open(Error &error) = 0;

	/**
	 * Close mixer device
	 */
	virtual void Close() = 0;

	/**
	 * Reads the current volume.
	 *
	 * @return the current volume (0..100 including) or -1 if
	 * unavailable or on error (error set, mixer will be closed)
	 */
	gcc_pure
	virtual int GetVolume(Error &error) = 0;

	/**
	 * Sets the volume.
	 *
	 * @param volume the new volume (0..100 including) @return
	 * true on success, false on error
	 */
	virtual bool SetVolume(unsigned volume, Error &error) = 0;
};

#endif
