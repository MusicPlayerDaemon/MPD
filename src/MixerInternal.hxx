/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include <glib.h>

class Mixer {
public:
	const struct mixer_plugin *plugin;

	/**
	 * This mutex protects all of the mixer struct, including its
	 * implementation, so plugins don't have to deal with that.
	 */
	GMutex *mutex;

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
	Mixer(const mixer_plugin &_plugin)
		:plugin(&_plugin),
		 mutex(g_mutex_new()),
		 open(false),
		 failed(false) {}

	bool IsPlugin(const mixer_plugin &other) const {
		return plugin == &other;
	}
};

#endif
