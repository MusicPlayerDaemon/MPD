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

#include "Configured.hxx"
#include "EncoderList.hxx"
#include "EncoderPlugin.hxx"
#include "config/Block.hxx"
#include "util/StringAPI.hxx"
#include "util/RuntimeError.hxx"

static const EncoderPlugin &
GetConfiguredEncoderPlugin(const ConfigBlock &block, bool shout_legacy)
{
	const char *name = block.GetBlockValue("encoder", nullptr);
	if (name == nullptr && shout_legacy)
		name = block.GetBlockValue("encoding", nullptr);

	if (name == nullptr)
		name = "vorbis";

	if (shout_legacy) {
		if (StringIsEqual(name, "ogg"))
			name = "vorbis";
		else if (StringIsEqual(name, "mp3"))
			name = "lame";
	}

	const auto plugin = encoder_plugin_get(name);
	if (plugin == nullptr)
		throw FormatRuntimeError("No such encoder: %s", name);

	return *plugin;
}

PreparedEncoder *
CreateConfiguredEncoder(const ConfigBlock &block, bool shout_legacy)
{
	return encoder_init(GetConfiguredEncoderPlugin(block, shout_legacy),
			    block);
}
