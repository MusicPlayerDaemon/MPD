/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#ifndef MPD_ENCODER_PLUGIN_HXX
#define MPD_ENCODER_PLUGIN_HXX

class PreparedEncoder;
struct ConfigBlock;
class Error;

struct EncoderPlugin {
	const char *name;

	PreparedEncoder *(*init)(const ConfigBlock &block,
				 Error &error);
};

/**
 * Creates a new encoder object.
 *
 * @param plugin the encoder plugin
 * @param error location to store the error occurring, or nullptr to ignore errors.
 * @return an encoder object on success, nullptr on failure
 */
static inline PreparedEncoder *
encoder_init(const EncoderPlugin &plugin, const ConfigBlock &block,
	     Error &error)
{
	return plugin.init(block, error);
}

#endif
