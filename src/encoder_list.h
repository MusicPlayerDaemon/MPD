/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MPD_ENCODER_LIST_H
#define MPD_ENCODER_LIST_H

struct encoder_plugin;

/**
 * Looks up an encoder plugin by its name.
 *
 * @param name the encoder name to look for
 * @return the encoder plugin with the specified name, or NULL if none
 * was found
 */
const struct encoder_plugin *
encoder_plugin_get(const char *name);

#endif
