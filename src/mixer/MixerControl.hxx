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

/** \file
 *
 * Functions which manipulate a #mixer object.
 */

#ifndef MPD_MIXER_CONTROL_HXX
#define MPD_MIXER_CONTROL_HXX

class Error;
class Mixer;
class EventLoop;
struct AudioOutput;
struct MixerPlugin;
class MixerListener;
struct config_param;

Mixer *
mixer_new(EventLoop &event_loop, const MixerPlugin &plugin, AudioOutput &ao,
	  MixerListener &listener,
	  const config_param &param,
	  Error &error);

void
mixer_free(Mixer *mixer);

bool
mixer_open(Mixer *mixer, Error &error);

void
mixer_close(Mixer *mixer);

/**
 * Close the mixer unless the plugin's "global" flag is set.  This is
 * called when the #AudioOutput is closed.
 */
void
mixer_auto_close(Mixer *mixer);

int
mixer_get_volume(Mixer *mixer, Error &error);

bool
mixer_set_volume(Mixer *mixer, unsigned volume, Error &error);

#endif
