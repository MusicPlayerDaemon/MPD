/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
 * Functions which affect the mixers of all audio outputs.
 */

#ifndef MPD_MIXER_ALL_H
#define MPD_MIXER_ALL_H

#include <stdbool.h>

/**
 * Returns the average volume of all available mixers (range 0..100).
 * Returns -1 if no mixer can be queried.
 */
int
mixer_all_get_volume(void);

/**
 * Sets the volume on all available mixers.
 *
 * @param volume the volume (range 0..100)
 * @return true on success, false on failure
 */
bool
mixer_all_set_volume(unsigned volume);

/**
 * Similar to mixer_all_get_volume(), but gets the volume only for
 * software mixers.  See #software_mixer_plugin.  This function fails
 * if no software mixer is configured.
 */
int
mixer_all_get_software_volume(void);

/**
 * Similar to mixer_all_set_volume(), but sets the volume only for
 * software mixers.  See #software_mixer_plugin.  This function cannot
 * fail, because the underlying software mixers cannot fail either.
 */
void
mixer_all_set_software_volume(unsigned volume);

#endif
