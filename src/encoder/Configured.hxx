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

#ifndef MPD_ENCODER_CONFIGURED_HXX
#define MPD_ENCODER_CONFIGURED_HXX

struct ConfigBlock;
class PreparedEncoder;

/**
 * Create a #PreparedEncoder instance from the settings in the
 * #ConfigBlock.  Its "encoder" setting is used to choose the encoder
 * plugin.
 *
 * Throws an exception on error.
 *
 * @param shout_legacy enable the "shout" plugin legacy configuration?
 * i.e. fall back to setting "encoding" instead of "encoder"
 */
PreparedEncoder *
CreateConfiguredEncoder(const ConfigBlock &block, bool shout_legacy=false);

#endif
